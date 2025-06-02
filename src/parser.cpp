#include "parser.h"
#include <sstream>
#include <algorithm>

//Выполняем синтаксический разбор блока program. Если во время разбора не обнаруживаем 
//никаких ошибок, то выводим последовательность команд стек-машины
void Parser::parse()
{
	program(); 
	if(!error_) {
		codegen_->flush();
	}
}

void Parser::program()
{
  // Резервируем место для команды
  // перехода на точку входа.
  codegen_->emit(NOP);

	functions();

  // Сохраняем адрес точки входа.
  // Он идёт сразу за последней
  // инструкцией последней функции.
  int entry_point = codegen_->getCurrentAddress();

	mustBe(T_BEGIN);
	statementList();
	mustBe(T_END);

	codegen_->emit(STOP);

  // Делаем переход на точку входа в
  // программу.
  codegen_->emitAt(0, JUMP, entry_point);

}

void Parser::statementList()
{
	//	  Если список операторов пуст, очередной лексемой будет одна из возможных "закрывающих скобок": END, OD, ELSE, FI.
	//	  В этом случае результатом разбора будет пустой блок (его список операторов равен null).
	//	  Если очередная лексема не входит в этот список, то ее мы считаем началом оператора и вызываем метод statement. 
	//    Признаком последнего оператора является отсутствие после оператора точки с запятой.
	if(see(T_END) || see(T_OD) || see(T_ELSE) || see(T_FI)) {
		return;
	}
	else {
		bool more = true;
		while(more) {
			statement();
			more = match(T_SEMICOLON);
		}
	}
}

void Parser::statement()
{
	// Если встречаем переменную, то запоминаем ее адрес или добавляем новую если не встретили. 
	// Следующей лексемой должно быть присваивание. Затем идет блок expression, который возвращает значение на вершину стека.
	// Записываем это значение по адресу нашей переменной
	if(see(T_IDENTIFIER)) {
    string varName = scanner_->getStringValue();
    bool new_var = false;
		int varAddress = findVariable(varName);
    bool is_local_variable = false;
    if (varAddress < 0)
    {
      new_var = true;
      varAddress = findOrAddVariable(varName);

      if (in_function)
      {
        is_local_variable = findParam(varName);
        // Резервируем место на стеке под переменную
        codegen_->emit(PUSH, 0);
      }

    }

		next();

    if (see(T_LSPAREN))
    {
      mustBe(T_LSPAREN);

      // Добавим возможность объявления массивов.
      // Для этого за идентификатором должно следовать
      // числовое значение - размер массива, заключённое
      // в квадратные скобки.
      if (new_var)
      {
        mustBe(T_NUMBER);
        int index = scanner_->getIntValue();

        mustBe(T_RSPAREN);

        // Объявляем переменную, с которой
        // ассоциирован массив перменной с
        // адресным типом.
        variables_[varName].type = ADDRESS;

        // Задаём место для массива
        lastVar_ = lastVar_ + index - 1;

        if (in_function)
        {
          for (int i = 0; i < index - 1; ++i)
          {
            codegen_->emit(PUSH, 100);
          }
        }
      }
      // Здесь мы присваиваем значение элементу массива
      // с определённым индексом.
      else
      {
        // Сохраняем на стеке значение,
        // лежащее в участке памяти,
        // отведённом под временное хранеине
        // значения выражения
        if (!in_function)
        {
          codegen_->emit(LOAD, lastVar_ + 1);
        }
        expression();

        mustBe(T_RSPAREN);

        // Сохраняем вычисленный индекс во
        // временную память
        if (!in_function)
        {
          codegen_->emit(STORE, lastVar_ + 1);
        }

        if (lastExpressionType_ != INTEGER)
        {
          reportError("index can't be an address"
              " variable.");
        }

        mustBe(T_ASSIGN);
        expression();

        // Загружаем из памяти значение
        // индекса, складываем его с
        // адресом начала массива и
        // кладём туда значение выражения.

        if (!is_local_variable &&
            in_function)
        {
          codegen_->emit(SLOAD, varAddress);
        }
        else
        {
          codegen_->emit(PUSH, varAddress);
        }

        if (in_function)
        {
          codegen_->emit(SLOAD, lastVar_);
          codegen_->emit(ADD);
          if (is_local_variable)
          {
            codegen_->emit(SBSTORE, 0);
          }
          else
          {
            codegen_->emit(BSTORE, 0);
          }

          codegen_->emit(POP);

        }
        else
        {
          codegen_->emit(LOAD, lastVar_ + 1);
          codegen_->emit(ADD);
          codegen_->emit(BSTORE, 0);

          // Восстанавливаем значение во
          // временной ячейке памяти
          codegen_->emit(STORE, lastVar_ + 1);
        }
        
      }
    }
    else if (see(T_LPAREN))
    {
      mustBe(T_LPAREN);

      int fn_address = findFunciton(varName);

      if (fn_address >= 0)
      {
        codegen_->emit(PUSH, 0);

        lastParamsTypes_ = functions_[varName].params_types;
        int n_args = lastParamsTypes_.size();

        arguments();

        lastParamsTypes_.clear();

        mustBe(T_RPAREN);

        codegen_->emit(BP);

        int offset = codegen_->getCurrentAddress() + 4;
        codegen_->emit(PUSH, offset);
        codegen_->emit(SSTORE, -n_args - 1);
        codegen_->emit(BP, -n_args);
        codegen_->emit(JUMP, functions_[varName].addr);
      }

    }
    else
    {
      mustBe(T_ASSIGN);

      expression();

      // Если переменная новая, то назначаем ей тип.
      // В противном случае, проверяем тип текущего выражения.
      // Если он соответствует типу переменной - выполняем
      // присваивание. В противном случае - ошибка.
      if (new_var)
      {
        variables_[varName].type = lastExpressionType_;
      }
      else if (lastExpressionType_ != variables_[varName].type)
      {
        reportError("mismatch expression and variable types.");
      }

      if (in_function)
      {
        codegen_->emit(SSTORE, varAddress);
      }
      else
      {
        codegen_->emit(STORE, varAddress);
      }
    }
	}
	// Если встретили IF, то затем должно следовать условие. На вершине стека лежит 1 или 0 в зависимости от выполнения условия.
	// Затем зарезервируем место для условного перехода JUMP_NO к блоку ELSE (переход в случае ложного условия). Адрес перехода
	// станет известным только после того, как будет сгенерирован код для блока THEN.
	else if(match(T_IF)) {
		relation();
		
		int jumpNoAddress = codegen_->reserve();

		mustBe(T_THEN);
		statementList();
		if(match(T_ELSE)) {
		//Если есть блок ELSE, то чтобы не выполнять его в случае выполнения THEN, 
		//зарезервируем место для команды JUMP в конец этого блока
			int jumpAddress = codegen_->reserve();
		//Заполним зарезервированное место после проверки условия инструкцией перехода в начало блока ELSE.
			codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
			statementList();
		//Заполним второй адрес инструкцией перехода в конец условного блока ELSE.
			codegen_->emitAt(jumpAddress, JUMP, codegen_->getCurrentAddress());
		}
		else {
		//Если блок ELSE отсутствует, то в зарезервированный адрес после проверки условия будет записана
		//инструкция условного перехода в конец оператора IF...THEN
			codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
		}

		mustBe(T_FI);
	}

	else if(match(T_WHILE)) {
		//запоминаем адрес начала проверки условия.
		int conditionAddress = codegen_->getCurrentAddress();
		relation();
		//резервируем место под инструкцию условного перехода для выхода из цикла.
		int jumpNoAddress = codegen_->reserve();
		mustBe(T_DO);
		statementList();
		mustBe(T_OD);
		//переходим по адресу проверки условия
		codegen_->emit(JUMP, conditionAddress);
		//заполняем зарезервированный адрес инструкцией условного перехода на следующий за циклом оператор.
		codegen_->emitAt(jumpNoAddress, JUMP_NO, codegen_->getCurrentAddress());
	}
	else if(match(T_WRITE)) {
		mustBe(T_LPAREN);
		expression();
		mustBe(T_RPAREN);
		codegen_->emit(PRINT);
	}
  else if (match(T_UNREF)) {
    // Если видем разыменование - значит хотим
    // что-то положить по адресу. Следовательно,
    // ожидаем определённую адресную переменную
    // (идентификатор) и оператор присваивания с
    // последующим выражением.
    
    if (see(T_IDENTIFIER))
    {
      mustBe(T_IDENTIFIER);
      string varName = scanner_->getStringValue();
      int varAddress = findVariable(varName);
      bool is_local_variable = false;
      if (in_function)
      {
        is_local_variable = findParam(varName);
      }

      // Если переменная определена и её тип - ADDRESS,
      // то разыменовываем и делаем тип выражения
      // целочисленным. Иначе - ошибка.
      if (varAddress >= 0 &&
          variables_[varName].type == ADDRESS) {

        mustBe(T_ASSIGN);
        expression();
        
        if (lastExpressionType_ == INTEGER)
        {
          // Нужно загрузить значение переменной
          // адресного типа, а затем по этому
          // значению положить значение в память
          if (in_function)
          {
            codegen_->emit(SLOAD, varAddress);
            if (is_local_variable)
            {
              codegen_->emit(SBSTORE, 0);
            }
            else
            {
              codegen_->emit(BSTORE, 0);
            }
          }
          else
          {
            codegen_->emit(LOAD, varAddress);
            codegen_->emit(BSTORE, 0);
          }
        }
        else
        {
          reportError("mismatch expression and"
              " variable types.");
        }
      }
      else
      {
        reportError("only defined address"
            " variable can be unrefered.");
      }
    }
    else if (see(T_LPAREN))
    {
      // Используем свободную память, чтобы
      // сохранить значение выражения. Перед
      // этим сохраняем её значение
      if (in_function)
      {
        codegen_->emit(SLOAD, lastVar_ + 1);
      }
      else
      {
        codegen_->emit(LOAD, lastVar_ + 1);
      }

      mustBe(T_LPAREN);
      expression();
      mustBe(T_RPAREN);

      // Сохраняем значение выражения,
      // выдающего адресс, по которому
      // положим значение следующего за
      // присваиванием выражения
      if (in_function)
      {
        codegen_->emit(SSTORE, lastVar_ + 1);
      }
      else
      {
        codegen_->emit(STORE, lastVar_ + 1);
      }

      if (lastExpressionType_ != ADDRESS)
      {
        reportError("only expression with"
            " address type can be unrefered.");
      }

      mustBe(T_ASSIGN);
      expression();

      if (lastExpressionType_ == INTEGER)
        {
          // Загружаем из памяти адресс,
          // по которому положим значение
          if (in_function)
          {
            codegen_->emit(SLOAD, lastVar_ + 1);
            codegen_->emit(SBSTORE, 0);

            codegen_->emit(SSTORE, lastVar_ + 1);
          }
          else
          {
            codegen_->emit(LOAD, lastVar_ + 1);
            codegen_->emit(BSTORE, 0);

            // Освобождаем занятую под значение
            // выражения память. Загружаем
            // запомненное в начале значение
            codegen_->emit(STORE, lastVar_ + 1);
          }
        }
        else
        {
          reportError("mismatch expression and"
              " variable types.");
        }
    }
    else
    {
      reportError("only variable or (<expression>)"
          " can be unrefered.");
    }

  }
	else {
		reportError("statement expected.");
	}
}

void Parser::expression()
{

	 /*
         Арифметическое выражение описывается следующими правилами: <expression> -> <term> | <term> + <term> | <term> - <term>
         При разборе сначала смотрим первый терм, затем анализируем очередной символ. Если это '+' или '-', 
		 удаляем его из потока и разбираем очередное слагаемое (вычитаемое). Повторяем проверку и разбор очередного 
		 терма, пока не встретим за термом символ, отличный от '+' и '-'
     */

  lastExpressionType_ = INTEGER;
	term();
	while(see(T_ADDOP)) {
		Arithmetic op = scanner_->getArithmeticValue();
		next();
		term();

		if(op == A_PLUS) {
			codegen_->emit(ADD);
		}
		else {
			codegen_->emit(SUB);
		}
	}
}

void Parser::term()
{
	 /*  
		 Терм описывается следующими правилами: <expression> -> <factor> | <factor> + <factor> | <factor> - <factor>
         При разборе сначала смотрим первый множитель, затем анализируем очередной символ. Если это '*' или '/', 
		 удаляем его из потока и разбираем очередное слагаемое (вычитаемое). Повторяем проверку и разбор очередного 
		 множителя, пока не встретим за ним символ, отличный от '*' и '/' 
	*/
	factor();
	while(see(T_MULOP)) {
		Arithmetic op = scanner_->getArithmeticValue();
		next();
		factor();

		if(op == A_MULTIPLY) {
			codegen_->emit(MULT);
		}
		else {
			codegen_->emit(DIV);
		}
	}
}

void Parser::factor()
{
	/*
		Множитель описывается следующими правилами:
		<factor> -> number | &identifier | *identifier |
   *(<expression>) | identifier | -<factor> |
   (<expression>) | READ | identifier(<parameters>)
	*/
	if(see(T_NUMBER)) {
		int value = scanner_->getIntValue();
		next();
		codegen_->emit(PUSH, value);
		//Если встретили число, то преобразуем его в целое и записываем на вершину стека
	}
  else if (see(T_REF)) {
    next();
    mustBe(T_IDENTIFIER);
    int varAddress = findVariable(scanner_->getStringValue());

    // Если переменная определена, то достаём её адресс,
    // иначе - синтаксическая ошибка
    if (varAddress >= 0)
    {
      lastExpressionType_ = ADDRESS;
      codegen_->emit(PUSH, varAddress);
    }
    else
    {
      reportError("only defined variable can be refered.");
    }
  }
  else if (see(T_UNREF)) {
    next();
    if (see(T_IDENTIFIER))
    {
      mustBe(T_IDENTIFIER);
      string varName = scanner_->getStringValue();
      int varAddress = findVariable(varName);
      bool is_local_variable = false;
      if (in_function)
      {
        is_local_variable = findParam(varName);
      }

      // Если переменная определена и её тип - ADDRESS,
      // то разыменовываем и делаем тип выражения
      // целочисленным. Иначе - ошибка.
      if (varAddress >= 0 &&
          variables_[varName].type == ADDRESS) {
        lastExpressionType_ = INTEGER;

        if (in_function)
        {
          codegen_->emit(SLOAD, varAddress);
          if (is_local_variable)
          {
            codegen_->emit(SBLOAD, 0);
          }
          else
          {
            codegen_->emit(BLOAD, 0);
          }
        }
        else
        {
          codegen_->emit(LOAD, varAddress);
          codegen_->emit(BLOAD, 0);
        }
      }
      else
      {
        reportError("only defined address"
            " variable can be unrefered.");
      }
    }
    else if (see(T_LPAREN))
    {
      mustBe(T_LPAREN);
      expression();
      mustBe(T_RPAREN);

      // Если тип выражения в скобках - ADDRESS,
      // то разыменовываем его и делаем тип 
      // глобального выражения
      // целочисленным. Иначе - ошибка.
      if (lastExpressionType_ == ADDRESS) {
        lastExpressionType_ = INTEGER;

        if (in_function)
        {
          codegen_->emit(SBLOAD, 0);
        }
        else
        {
          codegen_->emit(BLOAD, 0);
        }
      }
      else
      {
        reportError("only expression with address"
            " type can be unrefered.");
      }
    }
    else
    {
      reportError("only variable or (<expression>)"
          " can be unrefered.");
    }
  }
	else if(see(T_IDENTIFIER)) {
    string varName = scanner_->getStringValue();
		int varAddress = findVariable(varName);
    int fn_address = findFunciton(varName);
    bool is_local_variable = false;
    if (in_function)
    {
      is_local_variable = findParam(varName);
    }
    bool is_function = false;

		next();

    if (varAddress >= 0)
    {
      // Если обращение к элементу массива
      // произошло в выражении - нужно
      // достать значение из памяти по
      // заданному индексу
      if (see(T_LSPAREN))
      {
        next();

        // Получаем индекс
        expression();

        if (lastExpressionType_ != INTEGER)
        {
          reportError("index can't be an address"
              " variable.");
        }

        mustBe(T_RSPAREN);

        // Загружаем значение из памяти
        if (in_function)
        {
          if (is_local_variable)
          {
            codegen_->emit(SBLOAD, varAddress);
          }
          else
          {
            codegen_->emit(SLOAD, varAddress);
            codegen_->emit(ADD);
            codegen_->emit(BLOAD, 0);
          }
        }
        else
        {
          codegen_->emit(BLOAD, varAddress);
        }
      }
      else if (see(T_LPAREN))
      {
        mustBe(T_LPAREN);
        is_function = true;
      }
      else
      {
        if (variables_[varName].type == ADDRESS)
        {
          lastExpressionType_ = ADDRESS;
        }

        if (in_function)
        {
          codegen_->emit(SLOAD, varAddress);
        }
        else
        {
          codegen_->emit(LOAD, varAddress);
        }
      }
    }

    if (fn_address >= 0)
    {
      if (see(T_LPAREN) || is_function)
      {
        if (!is_function)
        {
          mustBe(T_LPAREN);
        }

        codegen_->emit(PUSH, 0);

        lastParamsTypes_ = functions_[varName].params_types;
        int n_args = lastParamsTypes_.size();

        arguments();

        lastParamsTypes_.clear();

        mustBe(T_RPAREN);

        codegen_->emit(BP);

        int offset = codegen_->getCurrentAddress() + 4;
        codegen_->emit(PUSH, offset);
        codegen_->emit(SSTORE, -n_args - 1);
        codegen_->emit(BP, -n_args);
        codegen_->emit(JUMP, functions_[varName].addr);
      }
      else
      {
        reportError("you should call a function"
            " using '(' and ')'.");
      }
    }

    if (fn_address < 0 && varAddress < 0)
    {
      reportError("only defined variable or function"
          " can be used in expression.");
    }
		//Если встретили переменную, то выгружаем значение, лежащее по ее адресу, на вершину стека 
	}
	else if(see(T_ADDOP) && scanner_->getArithmeticValue() == A_MINUS) {
		next();
		factor();
		codegen_->emit(INVERT);
		//Если встретили знак "-", и за ним <factor> то инвертируем значение, лежащее на вершине стека
	}
	else if(match(T_LPAREN)) {
		expression();
		mustBe(T_RPAREN);
		//Если встретили открывающую скобку, тогда следом может идти любое арифметическое выражение и обязательно
		//закрывающая скобка.
	}
	else if(match(T_READ)) {
		codegen_->emit(INPUT);
		//Если встретили зарезервированное слово READ, то записываем на вершину стека идет запись со стандартного ввода
	}
	else {
		reportError("expression expected.");
	}
}

void Parser::relation()
{
	//Условие сравнивает два выражения по какому-либо из знаков. Каждый знак имеет свой номер. В зависимости от 
	//результата сравнения на вершине стека окажется 0 или 1.
	expression();
	if(see(T_CMP)) {
		Cmp cmp = scanner_->getCmpValue();
		next();
		expression();
		switch(cmp) {
			//для знака "=" - номер 0
			case C_EQ:
				codegen_->emit(COMPARE, 0);
				break;
			//для знака "!=" - номер 1
			case C_NE:
				codegen_->emit(COMPARE, 1);
				break;
			//для знака "<" - номер 2
			case C_LT:
				codegen_->emit(COMPARE, 2);
				break;
			//для знака ">" - номер 3
			case C_GT:
				codegen_->emit(COMPARE, 3);
				break;
			//для знака "<=" - номер 4
			case C_LE:
				codegen_->emit(COMPARE, 4);
				break;
			//для знака ">=" - номер 5
			case C_GE:
				codegen_->emit(COMPARE, 5);
				break;
		};
	}
	else {
		reportError("comparison operator expected.");
	}
}

void Parser::arguments()
{
  int index = 0;
  int n_params = lastParamsTypes_.size();

  if (!see(T_RPAREN))
  {
    expression();

    if (lastExpressionType_ !=
        lastParamsTypes_[index++].type)
    {
      reportError("mismatch parameter's and"
          " argument's types.");
    }
  }

  while (!see(T_RPAREN))
  {
    mustBe(T_COMMA);
    expression();

    if (lastExpressionType_ !=
        lastParamsTypes_[index++].type)
    {
      reportError("mismatch parameter's and"
          " argument's types.");
    }
  }

  if (index != n_params)
  {
    reportError("wrong amount of arguments.");
  }
}

void Parser::parameters()
{
  Parameter param = {};

  bool is_reference = false;

  if (see(T_REF))
  {
    next();
    is_reference = true;
  }
  mustBe(T_IDENTIFIER);

  string varName = scanner_->getStringValue();
  findOrAddVariable(varName);
  VAR_TYPES type = (is_reference)? ADDRESS : INTEGER;
  variables_[varName].type = type;

  param = {varName, type};
  lastParamsTypes_.push_back(param);

  while(!see(T_RPAREN))
  {
    is_reference = false;
    mustBe(T_COMMA);

    if (see(T_REF))
    {
      next();
      is_reference = true;
    }
    mustBe(T_IDENTIFIER);

    varName = scanner_->getStringValue();
    findOrAddVariable(varName);
    VAR_TYPES type = (is_reference)? ADDRESS : INTEGER;
    variables_[varName].type = type;

    param = {varName, type};
    lastParamsTypes_.push_back(param);
  }
}

void Parser::functions()
{
  in_function = true;

  while (see(T_FUNCTION))
  {
    mustBe(T_FUNCTION);
    mustBe(T_IDENTIFIER);

    string fn_name = scanner_->getStringValue();
    VarTable variables;
    int lastVar = 0;

    VarTable variables_global = variables_;
    int lastVar_global = lastVar_;

    variables_ = variables;
    lastVar_ = lastVar;

    mustBe(T_LPAREN);
    parameters();
    mustBe(T_RPAREN);

    vector<Parameter> params_types = lastParamsTypes_;

    int addr = codegen_->getCurrentAddress();

    mustBe(T_BEGIN);
    statementList();
    mustBe(T_END);

    lastParamsTypes_.clear();
    variables = variables_;
    lastVar = lastVar_;

    addFunction(fn_name, addr,
        params_types, lastVar, variables);

    for (int i = 0; i < lastVar; ++i)
    {
      codegen_->emit(POP);
    }
    codegen_->emit(SJUMP);

    variables_ = variables_global;
    lastVar_ = lastVar_global;
  }

  in_function = false;
}

int Parser::findOrAddVariable(const string& var)
{
	VarTable::iterator it = variables_.find(var);
	if(it == variables_.end()) {
		variables_[var].addr = lastVar_;
		return lastVar_++;
	}
	else {
		return it->second.addr;
	}
}

int Parser::findVariable(const string& var)
{
  VarTable::iterator it = variables_.find(var);

	if(it != variables_.end()) {
    return it->second.addr;
	}
  else {
    /* TODO: remove magic numbers */
    return -1;
  }
}

int Parser::addFunction(const string& fn_name,
    const int addr, const vector<Parameter> params_types,
    const int lastVar, const VarTable variables)
{
	FuncTable::iterator it = functions_.find(fn_name);
	if(it == functions_.end()) {
		functions_[fn_name].addr = addr;
		functions_[fn_name].params_types = params_types;
		functions_[fn_name].lastVar = lastVar;
		functions_[fn_name].variables = variables;
		return functions_[fn_name].addr;
	}
	else {
		return -1;
	}
}

int Parser::findFunciton(const string& func)
{
  FuncTable::iterator it = functions_.find(func);

	if(it != functions_.end()) {
    return it->second.addr;
	}
  else {
    /* TODO: remove magic numbers */
    return -1;
  }
}

bool Parser::findParam(const string& param)
{
  return none_of(lastParamsTypes_.cbegin(),
      lastParamsTypes_.cend(),
      [param](Parameter p){return p.name == param;});
}

void Parser::mustBe(Token t)
{
	if(!match(t)) {
		error_ = true;

		// Подготовим сообщение об ошибке
		std::ostringstream msg;
		msg << tokenToString(scanner_->token()) << " found while " << tokenToString(t) << " expected.";
		reportError(msg.str());

		// Попытка восстановления после ошибки.
		recover(t);
	}
}

void Parser::recover(Token t)
{
	while(!see(t) && !see(T_EOF)) {
		next();
	}

	if(see(t)) {
		next();
	}
}
