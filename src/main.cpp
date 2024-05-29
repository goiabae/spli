#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#define RING_IMPL
#include "ring.hpp"

#define PERHAPS_IMPL
#include "perhaps.hpp"

using std::cout;
using std::string;
using std::vector;

struct File {
	File(const char* path, const char* mode) : m_fd {fopen(path, mode)} {}
	File(FILE* fd) : m_owned {false}, m_fd {fd} {}

	File(const File& other) : m_owned(false), m_fd(other.m_fd) {}
	File(File&& other) = default;
	File& operator=(const File& other) {
		if (this == &other) return *this;
		m_owned = false;
		m_fd = other.m_fd;
		return *this;
	}
	File& operator=(File&& other) = default;
	~File() {
		if (m_owned) (void)fclose(m_fd); // FIXME: handle fclose return value
	}

	bool operator!() { return m_fd == nullptr; }

	FILE* get_descriptor() { return m_fd; }
	bool at_eof() { return feof(m_fd) != 0; }
	char getc() { return static_cast<char>(fgetc(m_fd)); }

 private:
	bool m_owned {true};
	FILE* m_fd;
};

struct Token {
	enum class Type {
		PAREN_OPEN,
		PAREN_CLOSE,
		QUOTE,
		COMMA,
		GRAVE,
		SYM,
		STR,
		INT,
	};

	Token() : m_type(Type::GRAVE) {} // FIXME: set this to something else
	Token(Type type) : m_type(type) {}
	Token(Type type, string sym) : m_type(type), m_str(std::move(sym)) {}

	bool operator==(const Token& other) {
		return m_type == other.m_type && m_str == other.m_str;
	}

	Type type() { return m_type; }
	const string& sym() { return m_str; }

 private:
	Type m_type;
	std::string m_str {};
};

struct Lexer {
	struct Iterator {
		Iterator(Lexer* lexer) : lexer {lexer} { per = lexer->lex(); }
		Iterator() : lexer(nullptr) { per = Perhaps<Token>(); }
		bool operator!=(Iterator& other) { return !(per == other.per); }
		Iterator operator++() {
			per = lexer->lex();
			return *this;
		}
		Token operator*() { return per.unwrap(); }

	 private:
		Perhaps<Token> per;
		Lexer* lexer;
	};

	Iterator begin() { return this; }
	static Iterator end() { return {}; }

	Lexer(File& file) : file {file} {}

	Perhaps<Token> lex();

 private:
	File file;
	Ring<char> m_ring;

	static bool is_reserved(char c) {
		return isspace(c) != 0 || c == '(' || c == ')';
	}

	void ensure();
};

std::ostream& operator<<(std::ostream& st, Token& tk) {
	switch (tk.type()) {
		case Token::Type::PAREN_OPEN: st << '('; break;
		case Token::Type::PAREN_CLOSE: st << ')'; break;
		case Token::Type::SYM: st << tk.sym(); break;
		case Token::Type::COMMA: st << ','; break;
		case Token::Type::GRAVE: st << '`'; break;
		case Token::Type::QUOTE: st << '\''; break;
		case Token::Type::INT: st << tk.sym(); break;
		case Token::Type::STR: st << '"' << tk.sym() << '"'; break;
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
		case '"': {
			std::string str;
			per = m_ring.peek();
			while (per.is_some()) {
				c = per.unwrap();
				if (c == '"') {
					m_ring.read();
					return Token(Token::Type::STR, str);
				}
				str += c;
				m_ring.read();
				per = m_ring.peek();
			}
			return {};
		}
		case '\t':
		case '\n':
		case ' ': return lex();
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9': {
			std::string sym;
			sym += c;
			per = m_ring.peek();
			while (per.is_some()) {
				c = per.unwrap();
				if (isdigit(c) == 0) return Token {Token::Type::INT, sym};
				sym += c;
				m_ring.read();
				per = m_ring.peek();
			}
			return {};
		}
		case EOF: return {};
		default: {
			std::string sym;
			if (!is_reserved(c)) sym += c;
			for (Perhaps<char> per = m_ring.peek();
			     per.is_some() && !is_reserved(per.unwrap());
			     per = m_ring.peek()) {
				m_ring.read();
				sym += per.unwrap();
			}
			return Token {Token::Type::SYM, sym};
		}
	}
	return {};
}

void Lexer::ensure() {
	if (!m_ring.is_empty()) return;
	char c = EOF;
	while (!m_ring.is_full() && ((c = file.getc()), true) && c != EOF && c != '\n'
	) {
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
			STR,
		};
		Type type;

		union {
			vector<Node> children;
			Str sym;
			int num;
		};

		friend std::ostream& operator<<(std::ostream& st, AST::Node& node);

		Node& operator=(const Node& other) {
			if (this == &other) return *this;
			type = other.type;
			if (other.type == Type::SYM || other.type == Type::STR)
				new (&sym) std::string(other.sym);
			else if (type == Type::INT)
				num = other.num;
			else
				new (&children) vector<Node>(other.children);
			return *this;
		}

		Node& operator=(Node&& other) noexcept {
			type = other.type;
			if (other.type == Type::SYM || other.type == Type::STR)
				new (&sym) std::string(other.sym);
			else if (type == Type::INT)
				num = other.num;
			else
				new (&children) vector<Node>(other.children);
			return *this;
		}

		Node(const Node& other) : type(other.type) {
			if (other.type == Type::SYM || other.type == Type::STR)
				new (&sym) std::string(other.sym);
			else if (type == Type::INT)
				num = other.num;
			else
				new (&children) vector<Node>(other.children);
		}

		Node(Node&& other) noexcept : type(other.type) {
			if (other.type == Type::SYM || other.type == Type::STR)
				new (&sym) std::string(other.sym);
			else if (type == Type::INT)
				num = other.num;
			else
				new (&children) vector<Node>(other.children);
		}

		Node(Type type) : type(type), children() {}
		Node(Str str) : type(Type::SYM), sym(std::move(str)) {}
		Node(int num) : type(Type::INT), num(num) {}
		Node(Type type, Str str) : type(type), sym(std::move(str)) {}
		~Node() {
			if (type == Type::SYM || type == Type::STR)
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

	AST(Node _root) : root(std::move(_root)) {}

	Node root;
	friend std::ostream& operator<<(std::ostream& st, AST& ast);
};

std::ostream& operator<<(std::ostream& st, AST::Node& node) {
	switch (node.type) {
		case AST::Node::Type::CONS: {
			st << '(';
			if (!node.children.empty()) {
				for (size_t i = 0; i < node.children.size() - 1; i++)
					st << node.children[i] << ' ';
				st << node.children[node.children.size() - 1];
			}
			st << ')';
			break;
		}
		case AST::Node::Type::SYM: st << node.sym; break;
		case AST::Node::Type::QUOTE: st << '\'' << node.children[0]; break;
		case AST::Node::Type::QUASI: st << '`' << node.children[0]; break;
		case AST::Node::Type::UNQUOTE: st << ',' << node.children[0]; break;
		case AST::Node::Type::INT: st << node.num; break;
		case AST::Node::Type::STR: st << '"' << node.sym << '"'; break;
	}
	return st;
}

std::ostream& operator<<(std::ostream& st, AST& ast) { return st << ast.root; }

struct Parser {
	Lexer lexer;
	Parser(File& file) : lexer(file) {}
	Perhaps<AST> parse();

 private:
	Perhaps<AST::Node> parse_program();
	Perhaps<AST::Node> parse_exp();
	Perhaps<AST::Node> parse_list();
	Perhaps<AST::Node> parse_symbol();
	Perhaps<AST::Node> parse_quote();
	Perhaps<AST::Node> parse_quasi();
	Perhaps<AST::Node> parse_unquote();
	Perhaps<AST::Node> parse_int();
	Perhaps<AST::Node> parse_str();

	void ensure(); // ensure there are tokens in the ring to read from
	bool match(Token::Type type);
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
	if (tk.type() != type) return false;
	m_ring.read();
	return true;
}

void Parser::ensure() {
	if (!m_ring.is_empty()) return;
	for (auto per = lexer.lex(); !m_ring.is_full() && per.is_some();
	     per = lexer.lex())
		m_ring.write(per.unwrap());
}

Perhaps<AST::Node> operator||(
	Perhaps<AST::Node>&& lhs, Perhaps<AST::Node>&& rhs
) {
	if (lhs.is_some()) return std::move(lhs);
	return std::move(rhs);
}

Perhaps<AST> Parser::parse() {
	Perhaps<AST::Node> per = parse_program();
	if (per.is_none()) return {};
	AST ast(per.unwrap());
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
	if (sym.type() != Token::Type::SYM) return {};
	advance();
	return AST::Node(sym.sym());
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
	if (num.type() != Token::Type::INT) return {};
	advance();
	const int res = std::stoi(num.sym());
	return AST::Node(res);
}

// str = STR
Perhaps<AST::Node> Parser::parse_str() {
	auto per_str = peek();
	if (per_str.is_none()) return {};
	auto str = per_str.unwrap();
	if (str.type() != Token::Type::STR) return {};
	advance();
	return AST::Node(AST::Node::Type::STR, str.sym());
}

// exp = list | symbol | quote | quasi | unquote | int | str
Perhaps<AST::Node> Parser::parse_exp() {
	return parse_list() || parse_symbol() || parse_quote() || parse_quasi()
	    || parse_unquote() || parse_int() || parse_str();
}

// program = exp
Perhaps<AST::Node> Parser::parse_program() { return parse_exp(); }

void usage() { cout << "Usage:" << '\n' << "  spli <filepath>" << '\n'; }

int main(int argc, char* argv[]) {
	if (argc < 2) {
		usage();
		return 1;
	}

	File file = (argv[1][0] == '-') ? stdin : File(argv[1], "r");
	Parser parser(file);
	Perhaps<AST> per_ast = parser.parse();
	if (per_ast.is_none()) return 1;
	AST ast = per_ast.unwrap();
	cout << ast << '\n';
	return 0;
}
