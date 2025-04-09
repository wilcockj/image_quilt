gcc src/game.c -Iinc/ -c -g3 -lraylib -fPIC 2>error.log
gcc game.o -shared -o libgame.so
