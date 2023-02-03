/* * Data converted from D:/Espressif/PROJECTS/HTTP_SERVER_EG/main/LOGIN_CRED/password.txt
 * (null byte appended)
 */
.data
.section .rodata.embedded

.global password_txt
password_txt:

.global _binary_password_txt_start
_binary_password_txt_start: /* for objcopy compatibility */
.byte 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00

.global _binary_password_txt_end
_binary_password_txt_end: /* for objcopy compatibility */


.global password_txt_length
password_txt_length: /* not including null byte */
.word 9
