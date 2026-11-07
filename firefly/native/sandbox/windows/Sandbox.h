/** Native Windows launch boundary. Establish AppContainer/handle/job policy before resuming code.
 * No JS/Qt/LibWeb dependency; callers must supply owned, access-controlled resources.
 */
#pragma once
#ifndef _WIN32
#error Native Windows sandbox only
#endif
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>

namespace Firefly::WindowsSandbox {
struct Failure {
    char const* operation { "" };
    unsigned long code { 0 };
};

class Handle {
public:
    Handle() = default;

    explicit Handle(HANDLE value)
        : m_value(value)
    {
    }

    Handle(Handle const&) = delete;
    Handle& operator=(Handle const&) = delete;

    Handle(Handle&& other) noexcept
        : m_value(other.release())
    {
    }

    Handle& operator=(Handle&& other) noexcept;
    ~Handle();

    HANDLE get() const
    {
        return m_value;
    }

    explicit operator bool() const
    {
        return m_value && m_value != INVALID_HANDLE_VALUE;
    }

    HANDLE release();

    void reset(HANDLE = nullptr);

private:
    HANDLE m_value { nullptr };
};

class Profile {
public:
    static std::shared_ptr<Profile> create(Failure&);

    ~Profile();

    Profile(Profile const&) = delete;
    Profile& operator=(Profile const&) = delete;

    PSID sid() const
    {
        return m_sid;
    }

    std::wstring const& name() const
    {
        return m_name;
    }

    bool remove(Failure&);

private:
    friend class Child;
    Profile() = default;
    bool retain(Failure&);

    void release();

    std::mutex m_lock;
    unsigned m_children { 0 };
    std::wstring m_name;
    PSID m_sid { nullptr };
    bool m_registered { false };
};

struct Launch {
    std::wstring executable;
    std::wstring directory;
    std::vector<std::wstring> arguments;
    // Already-owned inheritable handles. Originals are never changed here.
    std::vector<HANDLE> inherited;
    HANDLE input { nullptr };
    HANDLE output { nullptr };
    HANDLE error { nullptr };
};

class Child {
public:
    // Returns suspended: the caller may independently inspect its native token
    // before resume(). Failure never falls back to an ordinary process.
    static std::unique_ptr<Child> create(std::shared_ptr<Profile>, Launch const&, Failure&);

    ~Child();

    Child(Child const&) = delete;
    Child& operator=(Child const&) = delete;

    DWORD pid() const
    {
        return m_pid;
    }

    HANDLE process() const
    {
        return m_process.get();
    }

    bool resume(Failure&);

    bool wait(DWORD timeout, DWORD& exit_code, Failure&);

private:
    Child() = default;
    std::shared_ptr<Profile> m_profile;
    Handle m_job;
    Handle m_process;
    Handle m_thread;
    DWORD m_pid { 0 };
    bool m_resumed { false };
};
}
