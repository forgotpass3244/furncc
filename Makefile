

CC = cc

# -Wall -Wextra -O3 -march=native -flto
CFLAGS  = -Wall -Wextra -O3 -march=native -flto #asan: -g -Wall -Wextra -fsanitize=address
LDFLAGS = -fsanitize=address #asan: -fsanitize=address

BUILDDIR = build

LEAFC_OBJS = \
	$(BUILDDIR)/Main.o \
	$(BUILDDIR)/Lexer.o \
	$(BUILDDIR)/Parser.o \
	$(BUILDDIR)/Compiler.o \
	$(BUILDDIR)/BytecodeBuilder.o # make a symlink if needed

all: $(BUILDDIR)/fcc

$(BUILDDIR)/fcc: $(LEAFC_OBJS)
	$(CC) $(LEAFC_OBJS) $(LDFLAGS) -o $@

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

