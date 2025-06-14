#ifndef CMILAN_PARSER_H
#define CMILAN_PARSER_H

#include "scanner.h"
#include "codegen.h"
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>

using namespace std;

/* Синтаксический анализатор.
 *
 * Задачи:
 * - проверка корректности программы,
 * - генерация кода для виртуальной машины в процессе анализа,
 * - простейшее восстановление после ошибок.
 *
 * Синтаксический анализатор языка Милан.
 * 
 * Парсер с помощью переданного ему при инициализации лексического анализатора
 * читает по одной лексеме и на основе грамматики Милана генерирует код для
 * стековой виртуальной машины. Синтаксический анализ выполняется методом
 * рекурсивного спуска.
 * 
 * При обнаружении ошибки парсер печатает сообщение и продолжает анализ со
 * следующего оператора, чтобы в процессе разбора найти как можно больше ошибок.
 * Поскольку стратегия восстановления после ошибки очень проста, возможна печать
 * сообщений о несуществующих ("наведенных") ошибках или пропуск некоторых
 * ошибок без печати сообщений. Если в процессе разбора была найдена хотя бы
 * одна ошибка, код для виртуальной машины не печатается.*/

class Parser 
{
public:
	// Конструктор
	//    const string& fileName - имя файла с программой для анализа
	//
	// Конструктор создает экземпляры лексического анализатора и генератора.

	Parser(const string& fileName, istream& input)
		: output_(cout), error_(false), recovered_(true), lastVar_(0)
	{
		scanner_ = new Scanner(fileName, input);
		codegen_ = new CodeGen(output_);
		next();
	}

	~Parser()
	{
		delete codegen_;
		delete scanner_;
	}

	void parse();	//проводим синтаксический разбор 

private:

  enum VAR_TYPES: char
  {
    INTEGER = 'i',
    ADDRESS = 'a',
  };

  typedef struct {
    int addr;
    enum VAR_TYPES type;
  } VarValue;

  typedef struct {
    string name;
    enum VAR_TYPES type;
  } Parameter;

	typedef map<string, VarValue> VarTable;

  typedef struct {
    int addr;
    int lastVar;
    bool is_returns;
    vector<Parameter> params_types;
    VarTable variables;
  } FunctionInfo;

	typedef map<string, FunctionInfo> FuncTable;

	//описание блоков.
	void program(); //Разбор программы. BEGIN statementList END
	void statementList(); // Разбор списка операторов.
	void statement(); //разбор оператора.
	void expression(); //разбор арифметического выражения.
	void term(); //разбор слагаемого.
	void factor(); //разбор множителя.
	void relation(); //разбор условия.
	void functions(); // Разбор списка функций.
	void parameters(); // Разбор списка параметров.
	void arguments(); // Разбор списка аргументов.

	// Сравнение текущей лексемы с образцом. Текущая позиция в потоке лексем не изменяется.
	bool see(Token t)
	{
		return scanner_->token() == t; 
	}

	// Проверка совпадения текущей лексемы с образцом. Если лексема и образец совпадают,
	// лексема изымается из потока.

	bool match(Token t)
	{
		if(scanner_->token() == t) {
			scanner_->nextToken();
			return true;
		}
		else {
			return false;
		}
	}

	// Переход к следующей лексеме.

	void next()
	{
		scanner_->nextToken();
	}

	// Обработчик ошибок.
	void reportError(const string& message)
	{
		cerr << "Line " << scanner_->getLineNumber() << ": " << message << endl;
		error_ = true;
	}
	
	void mustBe(Token t); //проверяем, совпадает ли данная лексема с образцом. Если да, то лексема изымается из потока.
	//Иначе создаем сообщение об ошибке и пробуем восстановиться
	void recover(Token t); //восстановление после ошибки: идем по коду до тех пор, 
	//пока не встретим эту лексему или лексему конца файла.
	int findOrAddVariable(const string&); //функция пробегает по variables_. 
	//Если находит нужную переменную - возвращает ее номер, иначе добавляет ее в массив, увеличивает lastVar и возвращает его.
	int findVariable(const string&); //функция пробегает по variables_. 
  int addFunction(const string& fn_name,
    const int addr, const bool is_returns,
    const vector<Parameter> params_types,
    const int lastVar, const VarTable variables);

	int findFunciton(const string&); //функция пробегает по functions_. 
	//Если находит нужную функцию - возвращает ее номер, иначе - ошибка.

	bool findParam(const string&);

	Scanner* scanner_; //лексический анализатор для конструктора
	CodeGen* codegen_; //указатель на виртуальную машину
	ostream& output_; //выходной поток (в данном случае используем cout)
	bool error_; //флаг ошибки. Используется чтобы определить, выводим ли список команд после разбора или нет
	bool recovered_; //не используется
	VarTable variables_; //массив переменных, найденных в программе
	int lastVar_; //номер последней записанной переменной
  enum VAR_TYPES lastExpressionType_;
  FuncTable functions_;
  bool in_function;

  vector<Parameter> lastParamsTypes_;
};

#endif
