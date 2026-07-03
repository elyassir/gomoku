NAME     = Gomoku

CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -I/home/yoyahya/goinfre/sfml/include -O3 -march=native
LDFLAGS  = -L/home/yoyahya/goinfre/sfml/lib -lsfml-graphics -lsfml-window -lsfml-system

SRC_DIR  = src
OBJ_DIR  = obj

SRCS     = $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/Board.cpp \
           $(SRC_DIR)/Game.cpp \
           $(SRC_DIR)/AI.cpp

OBJS     = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
