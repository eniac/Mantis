target = frontend

CPP_SRC=$(wildcard src/**/*.cpp)
CPP_H=$(wildcard include/*.h)
CPP_FLAGS=-Wno-deprecated-register

all: $(target)

$(target).tab.c $(target).tab.h:	$(target).y
	bison -d $(target).y

lex.yy.c: $(target).l $(target).tab.h
	flex $(target).l

$(target): $(CPP_SRC) $(CPP_H) lex.yy.c $(target).tab.c $(target).tab.h
	g++ -Wno-format -g -rdynamic -o $(target) $(target).tab.c lex.yy.c $(CPP_SRC) -std=c++11 $(CPP_FLAGS)

clean:
	rm -f $(target) $(target).tab.c lex.yy.c $(target).tab.h
	rm -rf $(target).dSYM/
