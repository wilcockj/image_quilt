gcc src/game.c src/heap.c -Iinc/ -c -g3 -O3 -lraylib -fPIC 2>error.log
gcc game.o heap.o -shared -o libgame.so
