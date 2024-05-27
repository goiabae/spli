#include <ctype.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#define RING_IMPL
#include "ring.hpp"

#define PERHAPS_IMPL
#include "perhaps.hpp"

using std::cout;
using std::endl;
using std::string;
using std::vector;

struct File {
	File(const char* path, const char* mode) : m_fd {fopen(path, mode)} {}
	File(FILE* fd) : m_fd {fd}, m_owned {false} {}
	~File() {
		if (m_owned) fclose(m_fd);
	}

	bool operator!() { return !m_fd; }

	FILE* get_descriptor() { return m_fd; }
	bool at_eof() { return feof(m_fd); }

 private:
	FILE* m_fd;
	bool m_owned {true};
};

struct Token {
	enum class Type {
		PAREN_OPEN,
		PAREN_CLOSE,
		QUOTE,
		COMMA,
		GRAVE,
		SYM,
		INT,
	};

	Type type;
	std::string sym {""};

	Token() {}
	Token(Type type) : type(type) {}
	Token(Type type, const string& sym) : type(type), sym(sym) {}

	bool operator==(Token other) {
		return type == other.type && sym == other.sym;
	}
};

struct Lexer {
	struct Iterator {
		Perhaps<Token> per;
		Lexer* lexer;
		Iterator(Lexer* lexer) : lexer {lexer} { per = lexer->lex(); }
		Iterator() { per = Perhaps<Token>(); }
		bool operator!=(Iterator& other) { return !(per == other.per); }
		Iterator operator++() {
			per = lexer->lex();
			return *this;
		}
		Token operator*() { return per.unwrap(); }
	};

	Iterator begin() { return Iterator(this); }
	Iterator end() { return Iterator(); }

	Lexer(File& file) : file {file} {}

	Perhaps<Token> lex();

 private:
	File& file;
	Ring<char> m_ring;

	bool is_reserved(char c) { return isspace(c) || c == '(' || c == ')'; }

	void ensure();
};

std::ostream& operator<<(std::ostream& st, Token& tk) {
	switch (tk.type) {
		case Token::Type::PAREN_OPEN: st << '('; break;
		case Token::Type::PAREN_CLOSE: st << ')'; break;
		case Token::Type::SYM: st << tk.sym; break;
		case Token::Type::COMMA: st << ','; break;
		case Token::Type::GRAVE: st << '`'; break;
		case Token::Type::QUOTE: st << '\''; break;
		case Token::Type::INT: st << tk.sym; break;
	}
	return st;
}

Perhaps<Token> Lexer::lex() {
	ensure();
	Perhaps<char> per = m_ring.read();
	if (per.is_none()) return {};
	char c = per.unwrap();
	switch (c) {
		case '(': return Token {Token::Type::PAREN_OPEN};
		case ')': return Token {Token::Type::PAREN_CLOSE};
		case '`': return Token {Token::Type::GRAVE};
		case '\'': return Token {Token::Type::QUOTE};
		case ',': return Token {Token::Type::COMMA};
		case '\t':
		case '\n':
		case ' ': return lex();
		case EOF: return {};
		default: {
			std::string sym = "";
			if (isdigit(c)) {
				sym += c;
				// m_ring.read();
				per = m_ring.peek();
				while (per.is_some()) {
					c = per.unwrap();
					if (!isdigit(c)) return Token {Token::Type::INT, sym};
					sym += c;
					m_ring.read();
					per = m_ring.peek();
				}
			}
			if (!is_reserved(c)) sym += c;
			Perhaps<char> per;
			while (((per = m_ring.peek()), true) && per.is_some()
			       && !is_reserved(per.unwrap())) {
				m_ring.read();
				sym += per.unwrap();
			}
			return Token {Token::Type::SYM, sym};
		}
	}
	return Perhaps<Token>();
}

void Lexer::ensure() {
	if (m_ring.len() != 0) return;
	char c;
	while (m_ring.len() < m_ring.cap()
	       && ((c = fgetc(file.get_descriptor())), true) && c != EOF
	       && c != '\n') {
		m_ring.write(c);
	}
}

using Number = int;
using Str = std::string;

struct AST {
	struct Node {
		enum class Type {
			CONS,
			SYM,
			QUOTE,
			QUASI,
			UNQUOTE,
			INT,
		};
		Type type;

		union {
			vector<Node> children;
			Str sym;
			int num;
		};

		friend std::ostream& operator<<(std::ostream& st, AST::Node& node);

		Node& operator=(const Node& other) {
			type = other.type;
			if (other.type == Type::SYM)
				new (&sym) std::string(other.sym);
			else if (type == Type::INT)
				num = other.num;
			else
				new (&children) vector<Node>(other.children);
			return *this;
		}
		Node(const Node& node) : type(node.type) {
			if (node.type == Type::SYM)
				new (&sym) std::string(node.sym);
			else if (type == Type::INT)
				num = node.num;
			else
				new (&children) vector<Node>(node.children);
		}
		// Node() : type(Type::INVALID) {}
		Node(Type type) : type(type), children() {}
		Node(Str str) : type(Type::SYM), sym(str) {}
		Node(int num) : type(Type::INT), num(num) {}
		~Node() {
			if (type == Type::SYM)
				sym.~Str();
			else if (type == Type::INT)
				(void)0;
			else
				children.~vector<Node>();
		}

		Node& operator+=(const Node& other) {
			assert(type != Type::SYM && type != Type::INT);
			children.push_back(other);
			return *this;
		}
	};

	Perhaps<Node> root;
	friend std::ostream& operator<<(std::ostream& st, AST& ast);
};

std::ostream& operator<<(std::ostream& st, AST::Node& node) {
	switch (node.type) {
		case AST::Node::Type::CONS: {
			st << '(';
			for (size_t i = 0; i < node.children.size() - 1; i++)
				st << node.children[i] << ' ';
			st << node.children[node.children.size() - 1];
			st << ')';
			break;
		}
		case AST::Node::Type::SYM: st << node.sym; break;
		case AST::Node::Type::QUOTE: st << '\'' << node.children[0]; break;
		case AST::Node::Type::QUASI: st << '`' << node.children[0]; break;
		case AST::Node::Type::UNQUOTE: st << ',' << node.children[0]; break;
		case AST::Node::Type::INT: st << node.num; break;
	}
	return st;
}

std::ostream& operator<<(std::ostream& st, AST& ast) {
	if (ast.root.is_none()) return st;
	AST::Node node = ast.root.unwrap();
	return st << node;
}

struct Parser {
	Lexer lexer;
	Parser(File& file) : lexer(file) {}
	AST parse();

 private:
	Perhaps<AST::Node> parse_program();
	Perhaps<AST::Node> parse_exp();
	Perhaps<AST::Node> parse_list();
	Perhaps<AST::Node> parse_symbol();
	Perhaps<AST::Node> parse_quote();
	Perhaps<AST::Node> parse_quasi();
	Perhaps<AST::Node> parse_unquote();
	Perhaps<AST::Node> parse_int();

	void ensure();
	bool match(Token::Type);
	Perhaps<Token> peek();
	Perhaps<Token> advance();

	Ring<Token> m_ring;
};

Perhaps<Token> Parser::peek() {
	ensure();
	return m_ring.peek();
}

Perhaps<Token> Parser::advance() {
	ensure();
	return m_ring.read();
}

bool Parser::match(Token::Type type) {
	ensure();
	auto per_tk = m_ring.peek();
	if (per_tk.is_none()) return false;
	auto tk = per_tk.unwrap();
	if (tk.type != type) return false;
	m_ring.read();
	return true;
}

void Parser::ensure() {
	if (m_ring.len() != 0) return;
	Perhaps<Token> per;
	while (m_ring.len() < m_ring.cap() && ((per = lexer.lex()), true)
	       && per.is_some()) {
		m_ring.write(per.unwrap());
	}
}

AST Parser::parse() {
	AST ast;
	Perhaps<AST::Node> res = parse_program();
	ast.root = res;
	return ast;
}

// quote = "'" exp
Perhaps<AST::Node> Parser::parse_quote() {
	if (!match(Token::Type::QUOTE)) return {};
	AST::Node quote(AST::Node::Type::QUOTE);
	auto exp = parse_exp();
	if (exp.is_some()) quote += exp.unwrap();
	return quote;
}

// quasi = "`" exp
Perhaps<AST::Node> Parser::parse_quasi() {
	if (!match(Token::Type::GRAVE)) return {};
	AST::Node quasi(AST::Node::Type::QUASI);
	auto exp = parse_exp();
	if (exp.is_some()) quasi += exp.unwrap();
	return quasi;
}

// unquote = "," exp
Perhaps<AST::Node> Parser::parse_unquote() {
	if (!match(Token::Type::COMMA)) return {};
	AST::Node unquote(AST::Node::Type::UNQUOTE);
	auto exp = parse_exp();
	if (exp.is_some()) unquote += exp.unwrap();
	return unquote;
}

// symbol = SYM
Perhaps<AST::Node> Parser::parse_symbol() {
	auto per_sym = peek();
	if (per_sym.is_none()) return {};
	auto sym = per_sym.unwrap();
	if (sym.type != Token::Type::SYM) return {};
	advance();
	return AST::Node(sym.sym);
}

// list = "(" exp* ")"
Perhaps<AST::Node> Parser::parse_list() {
	if (!match(Token::Type::PAREN_OPEN)) return {};

	AST::Node list(AST::Node::Type::CONS);
	while (true) {
		auto child = parse_exp();
		if (child.is_none()) break;
		list += child.unwrap();
	}

	if (!match(Token::Type::PAREN_CLOSE)) return {};
	return list;
}

// int = INT
Perhaps<AST::Node> Parser::parse_int() {
	auto per_num = peek();
	if (per_num.is_none()) return {};
	auto num = per_num.unwrap();
	if (num.type != Token::Type::INT) return {};
	advance();
	int res = std::stoi(num.sym);
	return AST::Node(res);
}

// exp = list | symbol | quote | quasi | unquote | int
Perhaps<AST::Node> Parser::parse_exp() {
	auto list = parse_list();
	if (list.is_some()) return list;
	auto symbol = parse_symbol();
	if (symbol.is_some()) return symbol;
	auto quote = parse_quote();
	if (quote.is_some()) return quote;
	auto quasi = parse_quasi();
	if (quasi.is_some()) return quasi;
	auto unquote = parse_unquote();
	if (unquote.is_some()) return unquote;
	auto _int = parse_int();
	if (_int.is_some()) return _int;
	return {};
}

// program = exp
Perhaps<AST::Node> Parser::parse_program() { return parse_exp(); }

void usage() { cout << "Usage:" << endl << "  spli <filepath>" << endl; }

int main(int argc, char* argv[]) {
	if (argc < 2) {
		usage();
		return 1;
	}
	File file = (argv[1][0] == '-') ? stdin : File(fopen(argv[1], "r"));
	Parser parser(file);
	AST ast = parser.parse();
	cout << ast << endl;
	return 0;
}
