TARGET   = tclc
CC       = gcc
VERSION  = 0.1
CPPFLAGS = -DVERSION=\"$(VERSION)\"
CFLAGS   = -std=c99 -pedantic -Wall -g -Wextra -Wswitch-default -Wno-empty-body\
	   -Wcast-align -Wundef $(CPPFLAGS)
INCS     = -I/opt/amdstream/include
LIBS     = -lOpenCL
LDFLAGS  = -L/opt/amdstream/lib/x86_64 $(LIBS)

SRC      = tclc.c
OBJ      = $(SRC:.c=.o)


all: $(OBJ)
	@echo -e '\033[1;33m'[LD] $(TARGET) '\033[1;m'
	@$(CC) -o $(TARGET) $(LDFLAGS) $(OBJ)


%.o: %.c
	@$(CC) -c $(CFLAGS) $<


dist: clean
	@mkdir -p $(TARGET)-$(VERSION)
	@cp LICENSE README Makefile $(SRC) $(TARGET)-$(VERSION)
	@tar cf $(TARGET)-$(VERSION).tar $(TARGET)-$(VERSION)
	@xz $(TARGET)-$(VERSION).tar
	@rm -rf $(TARGET)-$(VERSION)


clean:
	@rm -f $(TARGET) $(OBJ)


.PHONY: all dist clean
