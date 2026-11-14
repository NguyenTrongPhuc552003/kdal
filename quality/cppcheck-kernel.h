/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Kernel macros KDAL's C uses that cppcheck cannot expand without the
 * kernel's headers (it then skips the whole file as "unknownMacro").
 * Passed to cppcheck by the quality gate (quality/gate.json,
 * cppcheck.include); never compiled into kdal.ko. The loops follow the
 * kernel's definitions in include/linux/list.h closely enough for cppcheck
 * to read the code inside them.
 */
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - __builtin_offsetof(type, member)))

#define list_for_each_entry(pos, head, member)                          \
	for (pos = container_of((head)->next, __typeof__(*pos), member); \
	     &pos->member != (head);                                     \
	     pos = container_of(pos->member.next, __typeof__(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)                       \
	for (pos = container_of((head)->next, __typeof__(*pos), member),      \
	    n = container_of(pos->member.next, __typeof__(*pos), member);     \
	     &pos->member != (head);                                          \
	     pos = n, n = container_of(n->member.next, __typeof__(*n), member))
