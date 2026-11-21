//! The runtime's guest frame as one `microts::Input`.
//!
//! Buttons, relative axes and motion map directly. A touch contact becomes a
//! press target the way the guest framework treats a tap: the contact's hit
//! node, walked up to its nearest focusable ancestor at touch-down, is armed
//! (and shows `active:`); lifting the contact over the same target presses it
//! (CIRCLE edge with that target), lifting elsewhere cancels. Swipes and drags
//! are the application's: the C input source publishes the raw pointer as the
//! `pointerX`, `pointerY` and `pointerDown` bridge values.

use microts::{
    motion::{
        MotionAngles, MotionHeading, MotionOrientation, MotionPair, MotionScalar, MotionState,
        MotionVector,
    },
    spec::btn,
    Input, NodeId, Ui,
};
use pocket_runtime::GuestFrame;

const WIDE: u32 = 0x8000_0000;
const CANCEL: u32 = 0x4000_0000;

/// One contact of the guest frame's touch wire: logical x, y and its hit node.
fn first_contact(frame: &GuestFrame<'_>) -> Option<Option<(f32, f32, i32)>> {
    for (index, word) in frame.touches.iter().enumerate() {
        if frame.touch_surfaces.get(index).copied().unwrap_or(0) != 0 {
            continue;
        }
        if word & CANCEL != 0 {
            return Some(None);
        }
        let (x, y) = if word & WIDE != 0 {
            (word & 0x3FF, (word >> 10) & 0x3FF)
        } else {
            (word & 0x1FF, (word >> 9) & 0x1FF)
        };
        let hit = frame.hits.get(index).copied().unwrap_or(0);
        return Some(Some((x as f32, y as f32, hit)));
    }
    None
}

/// The nearest ancestor of `hit` (itself included) that the UI accepts as a
/// press target. `Ui` keeps its focusable set private, so each candidate is
/// offered to `resolve_input` with the buttons of the previous frame: no
/// button edge is produced or consumed, and an accepted target only gains
/// the `active` state that a press shows anyway.
fn press_target(ui: &mut Ui, hit: i32, held: u32) -> NodeId {
    let mut node = hit;
    let mut depth = 0;
    while node != 0 && depth < 64 {
        let probe = Input {
            buttons: held,
            target: NodeId(node),
            ..Input::default()
        };
        if ui.resolve_input(&probe).target == NodeId(node) {
            return NodeId(node);
        }
        if NodeId(node) == NodeId::ROOT {
            break;
        }
        node = ui.core().node_parent(node);
        depth += 1;
    }
    NodeId::NONE
}

/// Per-app input state across frames.
#[derive(Default)]
pub struct FrameInput {
    held: u32,
    touching: bool,
    armed: NodeId,
    last: (f32, f32),
}

impl FrameInput {
    pub fn translate(&mut self, frame: &GuestFrame<'_>, ui: &mut Ui) -> Input {
        let mut input = Input::buttons(frame.buttons);
        for delta in frame.axis_deltas {
            if delta.id < 2 {
                input = input.with_axis(delta.id as u8, delta.delta);
            }
        }
        if let Some(motion) = frame.motion {
            input = input.with_motion(motion_state(motion));
        }
        match first_contact(frame) {
            Some(Some((x, y, hit))) => {
                if !self.touching {
                    self.touching = true;
                    self.armed = press_target(ui, hit, self.held);
                }
                self.last = (x, y);
            }
            contact => {
                if self.touching {
                    self.touching = false;
                    let armed = core::mem::take(&mut self.armed);
                    if armed != NodeId::NONE {
                        let lifted_over = match contact {
                            None => {
                                let hit = ui.core_mut().hit_test_bounds(self.last.0, self.last.1);
                                press_target(ui, hit, self.held)
                            }
                            _ => NodeId::NONE, // cancelled
                        };
                        if lifted_over == armed {
                            input.target = armed;
                            input.buttons |= btn::CIRCLE;
                            input.pressed |= btn::CIRCLE;
                        } else {
                            // Clears the armed `active` state.
                            input.released |= btn::CIRCLE;
                        }
                    }
                }
            }
        }
        self.held = input.buttons;
        input
    }
}

fn vector<const N: usize>(value: [f32; N], quality: u8) -> MotionVector {
    let mut out = MotionVector {
        value: [0.0; 3],
        quality,
    };
    for (slot, component) in out.value.iter_mut().zip(value) {
        *slot = component;
    }
    out
}

/// The driver record of `pocket_drivers` as the MicroTS motion state; an
/// absent member keeps quality UNAVAILABLE (0).
pub fn motion_state(state: &pocket_drivers::MotionState) -> MotionState {
    let mut out = MotionState {
        timestamp: state.timestamp,
        ..MotionState::default()
    };
    if let Some(record) = state.gravity_direction {
        out.gravity_direction = vector(record.value, record.quality);
    }
    if let Some(record) = state.inclination {
        out.inclination = MotionScalar {
            value: record.value[0],
            quality: record.quality,
        };
    }
    if let Some(record) = state.linear_acceleration {
        out.linear_acceleration = vector(record.value, record.quality);
    }
    if let Some(record) = state.rotation_rate {
        out.rotation_rate = vector(record.value, record.quality);
    }
    if let Some(record) = state.orientation {
        out.orientation = MotionOrientation {
            value: record.value,
            quality: record.quality,
            reference_frame: record.reference_frame,
            epoch: record.epoch,
        };
    }
    if let Some(record) = state.heading {
        out.heading = MotionHeading {
            value: record.value[0],
            accuracy: record.value[1],
            quality: record.quality,
            reference_frame: record.reference_frame,
        };
    }
    if let Some(record) = state.screen_rotation {
        out.screen_rotation = MotionScalar {
            value: record.value[0],
            quality: record.quality,
        };
    }
    if let Some(record) = state.tilt {
        out.tilt = MotionPair {
            value: record.value,
            quality: record.quality,
        };
    }
    if let Some(record) = state.angles {
        out.angles = MotionAngles {
            value: record.value,
            quality: record.quality,
            reference_frame: record.reference_frame,
            epoch: record.epoch,
        };
    }
    out
}
