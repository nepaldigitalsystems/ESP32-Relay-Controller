/* * Data converted from D:/Espressif/PROJECTS/HTTP_SERVER_EG/main/LOGIN_CRED/username.txt
 * (null byte appended)
 */
.data
.section .rodata.embedded

.global username_txt
username_txt:

.global _binary_username_txt_start
_binary_username_txt_start: /* for objcopy compatibility */
.byte 0x41, 0x44, 0x4d, 0x49, 0x4e, 0x00

.global _binary_username_txt_end
_binary_username_txt_end: /* for objcopy compatibility */


.global username_txt_length
username_txt_length: /* not including null byte */
.word 5
