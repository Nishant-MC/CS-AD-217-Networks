# Defining our own build command
define build-cmd
$(CC) $(CLFAGS) $< -o $@
endef

# Compiler
CC = gcc

# More robust error checking at compile time
CFLAGS=-Wall -Werror -Wextra -Wshadow -Wunreachable-code -g -D_FORTIFY_SOURCE=2
LDFLAGS	= -lm

# Commenting out debug test code
# TESTDEFS	= -DTESTING

# Putting all .c files in a src folder			
SOURCE=src
VPATH=$(SOURCE)
MK_CHUNK_OBJS   = make_chunks.o chunk.o sha.o

BINS = peer make-chunks

# Commenting out buffer test code
# TESTBINS        = test_debug test_input_buffer

# Object files
OBJS= peer.o bt_parse.o spiffy.o debug.o input_buffer.o chunk.o sha.o

# New additions (Sliding Window, Connection Pool, Queue implementation, Packet implementation)
OBJS+=window.o
OBJS+=connPool.o
OBJS+=queue.o
OBJS+=packet.o
OBJS+=congestCtrl.o
OBJS+=sortedPacketCache.o

### COMMENTING OUT A WHOLE BUNCH OF THINGS

# Implicit .o target
# .c.o:
#	$(CC) $(TESTDEFS) -c $(CFLAGS) $<

# Explicit build and testing targets
# all: ${BINS} ${TESTBINS}

# run: peer_run
#	./peer_run

### COMMENTING OUT A WHOLE BUNCH OF THINGS


# Using our build command instead
$(SOURCE)/%.o: %.c
	$(build-cmd)


default: peer clean-o

peer: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# Deleting all .o files (We don't need them)
clean-o:
	rm -f *.o

make-chunks: $(MK_CHUNK_OBJS)
	$(CC) $(CFLAGS) $(MK_CHUNK_OBJS) -o $@ $(LDFLAGS)

# We're not using the test stuff any more
clean:
	rm -f *.o $(BINS) 

# The debugging utility code
debug-text.h: debug.h
	./debugparse.pl < debug.h > debug-text.h



### NOT USING THIS ANY MORE

#test_debug.o: debug.c debug-text.h
#	${CC} debug.c ${INCLUDES} ${CFLAGS} -c -D_TEST_DEBUG_ -o $@

#test_input_buffer:  test_input_buffer.o input_buffer.o

### NOT USING THIS ANY MORE
