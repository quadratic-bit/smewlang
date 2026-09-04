#ifndef COLORS_H
#define COLORS_H

#define ANSI_ESC "\x1b"
#define ANSI_CSI ANSI_ESC "["

#define CLR_END     ANSI_CSI "0m"
#define CLR_RED     ANSI_CSI "0;31m"
#define CLR_GREEN   ANSI_CSI "0;32m"
#define CLR_YELLOW  ANSI_CSI "0;33m"
#define CLR_BLUE    ANSI_CSI "0;34m"
#define CLR_MAGENTA ANSI_CSI "0;35m"
#define CLR_CYAN    ANSI_CSI "0;36m"

#endif
