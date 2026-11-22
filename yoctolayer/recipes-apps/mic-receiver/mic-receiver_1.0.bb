SUMMARY = "Reads PDM microphone statistics from an nRF52840 over UART"
DESCRIPTION = "Opens a serial port at 115200 8N1 and prints the lines that contain 'mic:'."
LICENSE = "CLOSED"

SRC_URI = "file://mic-receiver.c"
S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o mic-receiver mic-receiver.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 mic-receiver ${D}${bindir}/mic-receiver
}
