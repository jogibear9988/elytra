/** Read a compositor frame without blocking the Qt event loop during a native probe. */
#pragma once

#include <LibGfx/Bitmap.h>
#include <QImage>
#include <UI/Qt/WebContentView.h>
#include <memory>

namespace Firefly::Tests {
class RenderedFrameSnapshot {
public:
    QImage request(Ladybird::WebContentView& view)
    {
        if (!m_state->pending) {
            m_state->pending = true;
            view.take_frame_snapshot([state = m_state](Optional<Gfx::ShareableBitmap> snapshot) {
                state->pending = false;
                auto const* bitmap
                    = snapshot.has_value() && snapshot->is_valid() ? snapshot->bitmap() : nullptr;
                if (!bitmap) {
                    state->image = {};
                    return;
                }

                QImage borrowed(bitmap->scanline_u8(0), bitmap->width(), bitmap->height(),
                    bitmap->pitch(), QImage::Format_RGB32);

                state->image = borrowed.copy();
            });
        }

        return m_state->image;
    }

private:
    struct State {
        bool pending { false };
        QImage image;
    };

    std::shared_ptr<State> m_state { std::make_shared<State>() };
};
}
