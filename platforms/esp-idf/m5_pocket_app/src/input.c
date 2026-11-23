#include "app_internal.h"
#include "profile.h"
#include <string.h>

/* The program's input callbacks as one pocket_input_source_v1. The runtime
 * polls it once per frame on the app task, in both modes. */

static uint32_t classes(void *context) {
  const m5_pocket_input_config *input = context;
  uint32_t bits = POCKET_INPUT_CLASS_BUTTONS;
  if (input->touch)
    bits |= POCKET_INPUT_CLASS_CONTACT;
  if (input->axes)
    bits |= POCKET_INPUT_CLASS_AXIS_RELATIVE;
  if (input->motion)
    bits |= POCKET_INPUT_CLASS_MOTION;
  return bits;
}

static int32_t poll(void *context, pocket_input_frame_builder *builder) {
  const m5_pocket_input_config *input = context;
  if (!input || !builder || builder->struct_size < sizeof(*builder) ||
      !builder->append)
    return POCKET_STATUS_INVALID;
  m5_pocket_profile_stage(M5_POCKET_STAGE_INPUT);
  pocket_input_frame frame = {.struct_size = sizeof(frame)};
  if (input->buttons)
    frame.buttons = input->buttons(input->context);

  pocket_contact contact = {.struct_size = sizeof(contact), .id = 1};
  float x = 0, y = 0;
  bool down = input->touch && input->touch(input->context, &x, &y);
  /* The profiler console's scripted contact and buttons. */
  m5_pocket_profile_input(&frame.buttons, &down, &x, &y);
#if CONFIG_M5_POCKET_APP_PROFILE
  static bool was_down;
  static uint32_t held_buttons;
  if (down != was_down)
    m5_pocket_profile_note("contact %s %d %d", down ? "down" : "up", (int)x,
                           (int)y);
  if (frame.buttons != held_buttons)
    m5_pocket_profile_note("buttons %#x", (unsigned)frame.buttons);
  was_down = down;
  held_buttons = frame.buttons;
#endif
  if (input->touch) {
    if (down) {
      contact.x = x;
      contact.y = y;
      frame.contacts = &contact;
      frame.contact_count = 1;
      m5_pocket_bridge_set_number("pointerX", x);
      m5_pocket_bridge_set_number("pointerY", y);
    }
    m5_pocket_bridge_set_bool("pointerDown", down);
  }

  pocket_axis_delta deltas[2] = {
      {.struct_size = sizeof(pocket_axis_delta), .axis = 0},
      {.struct_size = sizeof(pocket_axis_delta), .axis = 1},
  };
  if (input->axes) {
    int32_t values[2] = {0, 0};
    input->axes(input->context, values);
    deltas[0].delta = values[0];
    deltas[1].delta = values[1];
    if (values[0] || values[1]) {
      frame.relative = deltas;
      frame.relative_count = 2;
    }
  }

  pocket_motion_state motion;
  if (input->motion) {
    memset(&motion, 0, sizeof(motion));
    motion.struct_size = sizeof(motion);
    if (input->motion(input->context, &motion))
      frame.motion = &motion;
  }
  m5_pocket_profile_stage(M5_POCKET_STAGE_PRE);
  return builder->append(builder->context, &frame);
}

static const pocket_input_source_v1 SOURCE = {
    .struct_size = sizeof(SOURCE),
    .classes = classes,
    .poll = poll,
};

const pocket_input_source_v1 *m5_pocket_input_vtable(void) { return &SOURCE; }
