# Bundled static FFmpeg belongs to LibMediaCodecs, not its ELF public ABI. Its x86 assembly uses
# direct PC-relative references to internal tables; allowing those archive symbols to be preempted
# makes a shared-library link invalid. Hide only the bundled codec archives, preserving the
# component's public API and upstream's separately dlopened system-FFmpeg provider.
function(firefly_hide_bundled_ffmpeg target)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(
            ${target}
            PRIVATE
            "LINKER:--exclude-libs=libavcodec-ladybird.a:libavutil-ladybird.a:libswresample-ladybird.a"
        )
    endif()
endfunction()
