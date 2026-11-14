#pragma once

namespace Piccolo
{
    // Selects locomotion phases from motor contacts and one-shot playback completion.
    // Idle and moving jumps use the same clips in the shipped course assets.
    class AnimationFSM
    {
    public:
        enum class State
        {
            idle,
            walk_start,
            walk_run,
            walk_stop,
            jump_start,
            jump_loop,
            jump_end
        };

        bool        update(bool is_moving, bool is_jumping, bool clip_finished);
        const char* getStateName() const;
        bool        isLooping() const;

    private:
        State m_state {State::idle};
    };
} // namespace Piccolo
