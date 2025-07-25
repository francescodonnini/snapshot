#include "kretprobe_handlers.h"

int submit_bio_entry_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}

int submit_bio_handler(struct kretprobe_instance *kp, struct pt_regs *regs) {
    return 0;
}