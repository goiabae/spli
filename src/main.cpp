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
	};

	Type type;
	std::string sym;

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
		default: {
			std::string sym = "";
			if (c == EOF) return Perhaps<Token>();
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
			EVAL,
		};
		Type type;

		union {
			vector<Node> children;
			Str sym;
		};

		friend std::ostream& operator<<(std::ostream& st, AST::Node& node);

		Node& operator=(const Node& other) {
			type = other.type;
			if (other.type == Type::SYM)
				new (&sym) std::string(other.sym);
			else
				new (&children) vector<Node>(other.children);
			return *this;
		}
		Node(const Node& node) : type(node.type) {
			if (node.type == Type::SYM)
				new (&sym) std::string(node.sym);
			else
				new (&children) vector<Node>(node.children);
		}
		// Node() : type(Type::INVALID) {}
		Node(Type type) : children(), type(type) {}
		Node(Str str) : sym(str), type(Type::SYM) {}
		~Node() {
			if (type == Type::SYM)
				sym.~Str();
			else
				children.~vector<Node>();
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
		case AST::Node::Type::EVAL: st << ',' << node.children[0]; break;
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
	Perhaps<AST::Node> parse_node();
	void ensure();
	bool match(Token::Type);
	Ring<Token> m_ring;
};

bool Parser::match(Token::Type type) {
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
	Perhaps<AST::Node> res = parse_node();
	ast.root = res;
	return ast;
}

Perhaps<AST::Node> Parser::parse_node() {
	ensure();
	auto per = m_ring.peek();
	if (per.is_none()) return {};
	Token tk = per.unwrap();
	switch (tk.type) {
		case Token::Type::PAREN_OPEN: {
			m_ring.read();
			AST::Node res(AST::Node::Type::CONS);
			while (true) {
				auto child = parse_node();
				if (child.is_none()) break;
				res.children.push_back(child.unwrap());
			}
			if (!match(Token::Type::PAREN_CLOSE)) return {};
			return res;
		}
		case Token::Type::PAREN_CLOSE: return {};
		case Token::Type::COMMA: {
			m_ring.read();
			AST::Node res(AST::Node::Type::EVAL);
			auto list = parse_node();
			if (list.is_some()) res.children.push_back(list.unwrap());
			return res;
		}
		case Token::Type::GRAVE: {
			m_ring.read();
			AST::Node res(AST::Node::Type::QUASI);
			auto list = parse_node();
			if (list.is_some()) res.children.push_back(list.unwrap());
			return res;
		}
		case Token::Type::QUOTE: {
			m_ring.read();
			AST::Node res(AST::Node::Type::QUOTE);
			auto list = parse_node();
			if (list.is_some()) res.children.push_back(list.unwrap());
			return res;
		}
		case Token::Type::SYM: {
			m_ring.read();
			AST::Node res(tk.sym);
			return res;
		}
	}
}

int main(int argc, char* argv[]) {
	File file = (argv[1][0] == '-') ? stdin : File(fopen(argv[1], "r"));
	Parser parser(file);
	AST ast = parser.parse();
	cout << ast << endl;
	return 0;
}
