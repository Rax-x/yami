#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <functional>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <cstdarg>
 
template<typename Base, typename T>
constexpr inline bool instanceof(const T *ptr) {
   return dynamic_cast<const Base*>(ptr) != nullptr;
}

class Erronable {
	public:
		Erronable() : hadError_(false) { }
		virtual ~Erronable() = default;
	
		inline bool anErrorOccurred() const {
			return hadError_;
		}

	protected:
		void emitError(const char* fmt, ...){
			va_list args;
			va_start(args, fmt);
			
			std::printf(fmt, args);			
			
			va_end(args);	
			hadError_ = true;
		}

	private:
		bool hadError_;

};

enum class TokenType : std::uint8_t {
	number,
	minus,
	plus,
	slash,
	star,
	eof
};

struct Token {
	explicit Token(TokenType type, const std::string& lexeme)
		: type_(type), lexeme_(std::move(lexeme)) { }

	TokenType type_;
	std::string lexeme_;
};

class Tokenizer : public Erronable {
	public:
		Tokenizer(std::string source) 
			: source_(std::move(source)) {}
		~Tokenizer() = default;

		std::vector<Token> lex() {	
			auto end = source_.end();
			
			for(auto it = source_.begin(); it < end ; it++){
				
				if(std::isspace(*it)) continue;

				if(std::isdigit(*it)){
					auto start = it;
					lexNumber(it, end);
					tokens_.emplace_back(TokenType::number, std::string(start, it));			
					continue;
				}

				switch(*it){
					case '+':
						tokens_.emplace_back(TokenType::plus, std::string(1, *it));
						break;	
					case '-':
						tokens_.emplace_back(TokenType::minus, std::string(1, *it));	
						break;
					case '*':
						tokens_.emplace_back(TokenType::star, std::string(1, *it));
						break;
					case '/':	
						tokens_.emplace_back(TokenType::slash, std::string(1, *it));
						break;
					default: {
						std::uint32_t position  = it - source_.begin();
						emitError("Unexpectd character at column %u: '%c'\n", position, *it);
						return tokens_;
					}
				}
			}

			tokens_.emplace_back(TokenType::eof, std::string(1, '\0'));

			return tokens_;
		}
	private:

		void lexNumber(std::string::iterator& it, const std::string::iterator& end){
			while(std::isdigit(*it) && it < end) it++;

			if(*it == '.') {
				it++;
				while(std::isdigit(*it) && it < end) it++;
			}

			if(*it != 'e') return; 
			
			it++;
			if(*it == '-' || *it == '+') it++;
			
			while(std::isdigit(*it) && it < end) it++;
		}

		std::string source_;
		std::vector<Token> tokens_;
};

enum Precedence {
	NONE = 0,
	TERM,
	FACTOR,
	UNARY,
	PRIMARY
};

class Expr {
	public:
		virtual ~Expr() = default;
};

class BinaryExpr : public Expr {
	public:
		explicit BinaryExpr(TokenType op, Expr* left, Expr* right)
			: op_(op), left_(left), right_(right) { }

		TokenType op_;
		std::unique_ptr<Expr> left_;
		std::unique_ptr<Expr> right_;
};

class UnaryExpr : public Expr {
	public:
		explicit UnaryExpr(TokenType op, Expr* right)
			: op_(op), right_(right) { } 

		TokenType op_;
		std::unique_ptr<Expr> right_;
};

class LiteralExpr : public Expr { 
	public:
		explicit LiteralExpr(double value)
			: value_(value) { }

		double value_;
};


class PrattParser; // forward

typedef Expr*(PrattParser::*ParseFnInfix)(Expr*);
typedef Expr*(PrattParser::*ParseFnPrefix)();

struct ParseRule {
	Precedence precedence;
	ParseFnInfix infix;
	ParseFnPrefix prefix;
};

class PrattParser : public Erronable {

	public:
		PrattParser(const std::vector<Token>& tokens)
				: tokens_(std::move(tokens)) {
			   
			registerAllRules();
		}

		std::shared_ptr<Expr> parse() {
			return std::shared_ptr<Expr>(expression());
		}

	private:

		inline Token& peek(){
			return tokens_[current_];
		}

		inline bool isAtEnd() {
			return peek().type_ == TokenType::eof;
		}

		inline Token& previous(){
			return tokens_[current_-1];
		}

		Token& advance(){
			if(!isAtEnd()) current_++;
			return previous();	
		}


		void registerRule(TokenType type, Precedence prec, ParseFnPrefix prefix, ParseFnInfix infix){
			rules_.insert(std::pair<TokenType, ParseRule>(
				type,
				{	
					.precedence = prec,
		  			.infix = infix,
					.prefix = prefix				
				}
			));
		}

		void registerAllRules(){	
			registerRule(TokenType::plus, TERM, &PrattParser::unary, &PrattParser::binary);
			registerRule(TokenType::minus, TERM, &PrattParser::unary, &PrattParser::binary);
			registerRule(TokenType::star, FACTOR, nullptr, &PrattParser::binary);
			registerRule(TokenType::slash, FACTOR, nullptr, &PrattParser::binary);
			registerRule(TokenType::number, PRIMARY, &PrattParser::primary, nullptr);
			registerRule(TokenType::eof, NONE, nullptr, nullptr);
		}

		ParseRule& getRule(TokenType type) {
			return rules_[type];
		}

		Expr* parsePrecedence(Precedence precedence){
			
			auto& token = advance();
			auto prefixRule = getRule(token.type_).prefix; 

			if(prefixRule == nullptr){
				emitError(
					"Expect expression.\n"
					"Unable to parse: '%s'\n", token.lexeme_.c_str());

				return nullptr;
			}

			auto expr = (this->*prefixRule)();
			
			while(precedence <= getRule(peek().type_).precedence){
				auto token = advance();
				auto infixRule = getRule(token.type_).infix;
				expr = (this->*infixRule)(expr);
			}			

			return expr;
		}
			
		Expr* expression(){
			return parsePrecedence(TERM);
		}

		Expr* binary(Expr* left){
			auto& op = previous();
			auto right = parsePrecedence((Precedence)(getRule(op.type_).precedence + 1));
			return new BinaryExpr(op.type_, left, right);
		}

		Expr* unary(){
			auto& op = previous();
			auto right = parsePrecedence(UNARY);	
			return new UnaryExpr(op.type_, right);
		}

		Expr* primary(){

			auto token = previous();

			if(token.type_ == TokenType::number){
				return new LiteralExpr(std::strtod(token.lexeme_.c_str(), nullptr));
			}

			emitError("Expect a number literal\n");
			return nullptr;
		}

		std::unordered_map<TokenType, ParseRule> rules_;
		
		std::uint32_t current_ = 0;
		std::vector<Token> tokens_;
};

class Evaluator : public Erronable {
	
	public:
		Evaluator(Expr* ast)
			: ast_(ast) { }

		inline double eval(){
			return evaluateExpression(ast_);
		}

	private:
		
		double evaluateExpression(Expr* expr){
			if(instanceof<BinaryExpr, Expr>(expr)){
				auto e = static_cast<BinaryExpr*>(expr);
				double left = evaluateExpression(e->left_.get());
				double right = evaluateExpression(e->right_.get());

				switch(e->op_){
					case TokenType::plus:
						return left + right;
					case TokenType::minus:
						return left - right;
					case TokenType::star:
						return left * right;
					case TokenType::slash:
						return left / right;
					default:
						emitError("Invalid binary operator!\n");
						break;
				}
			}else if(instanceof<UnaryExpr, Expr>(expr)){
				auto e = static_cast<UnaryExpr*>(expr);
				double right = evaluateExpression(e->right_.get());
				switch(e->op_){
					case TokenType::plus:
						return right;
					case TokenType::minus:
						return -right;
					default:
						emitError("Invalid unary operator!\n");
						break;
				}

			}else{
				if(instanceof<LiteralExpr, Expr>(expr)){
					return static_cast<LiteralExpr*>(expr)->value_;
				}
			}

			return NAN;
		}

		Expr* ast_;
};

int main(void){

	std::string line;
	
	while(true){
		std::cout << "evaluator -> ";
		std::getline(std::cin, line);
		if(line == "exit") break;

		auto tokenizer = Tokenizer(line);
		auto tokens = std::move(tokenizer.lex());
		
		if(tokenizer.anErrorOccurred()) continue;

		auto parser = PrattParser(tokens);
		
		auto ast = parser.parse();
		if(parser.anErrorOccurred()) continue;

		auto evaluator = Evaluator(ast.get());
		std::cout << evaluator.eval() << std::endl;	
	}

	return 0;
}
