CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
LDFLAGS = -lm

SRC      = linkbudget_core.c
TEST_SRC = test_linkbudget.c

# All GUI translation units
GUI_SRC  = Link Budget Calculator.c \
           linkbudget_ui_controls.c \
           linkbudget_ui_logic.c \
           linkbudget_ui_io.c \
           linkbudget_pdf.c

gui: $(GUI_SRC) $(SRC) app.res
	$(CC) $(CFLAGS) $(GUI_SRC) $(SRC) app.res -o Link Budget Calculator -lcomctl32 -mwindows $(LDFLAGS)

app.res: app.rc app.ico
	windres app.rc -O coff -o app.res

test: $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) -o test_linkbudget $(LDFLAGS)
	./test_linkbudget

clean:
	rm -f test_linkbudget test_linkbudget.exe Link Budget Calculator.exe Link Budget Calculator app.res

.PHONY: gui test clean
