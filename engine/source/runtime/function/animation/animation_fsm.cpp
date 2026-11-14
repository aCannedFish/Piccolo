#include "runtime/function/animation/animation_fsm.h"

namespace Piccolo
{
    bool AnimationFSM::update(bool is_moving, bool is_jumping, bool clip_finished)
    {
        const State previous = m_state;
        switch (m_state)
        {
            case State::idle:
                if (is_jumping)
                {
                    m_state = State::jump_start;
                }
                else if (is_moving)
                {
                    m_state = State::walk_start;
                }
                break;
            case State::walk_start:
                if (is_jumping)
                {
                    m_state = State::jump_start;
                }
                else if (!is_moving)
                {
                    m_state = State::walk_stop;
                }
                else if (clip_finished)
                {
                    m_state = State::walk_run;
                }
                break;
            case State::walk_run:
                if (is_jumping)
                {
                    m_state = State::jump_start;
                }
                else if (!is_moving)
                {
                    m_state = State::walk_stop;
                }
                break;
            case State::walk_stop:
                if (is_jumping)
                {
                    m_state = State::jump_start;
                }
                else if (is_moving)
                {
                    m_state = State::walk_start;
                }
                else if (clip_finished)
                {
                    m_state = State::idle;
                }
                break;
            case State::jump_start:
                if (!is_jumping)
                {
                    m_state = State::jump_end;
                }
                else if (clip_finished)
                {
                    m_state = State::jump_loop;
                }
                break;
            case State::jump_loop:
                if (!is_jumping)
                {
                    m_state = State::jump_end;
                }
                break;
            case State::jump_end:
                if (is_jumping)
                {
                    m_state = State::jump_start;
                }
                else if (clip_finished)
                {
                    m_state = is_moving ? State::walk_run : State::idle;
                }
                break;
        }
        return previous != m_state;
    }

    const char* AnimationFSM::getStateName() const
    {
        switch (m_state)
        {
            case State::idle:
                return "idle";
            case State::walk_start:
                return "walk_start";
            case State::walk_run:
                return "walk_run";
            case State::walk_stop:
                return "walk_stop";
            case State::jump_start:
                return "jump_start";
            case State::jump_loop:
                return "jump_loop";
            case State::jump_end:
                return "jump_end";
        }
        return "idle";
    }

    bool AnimationFSM::isLooping() const
    {
        return m_state == State::idle || m_state == State::walk_run || m_state == State::jump_loop;
    }
} // namespace Piccolo
