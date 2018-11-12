#include <handlebars/dispatcher.hpp>
#include <handlebars/handler.hpp>
#include <iostream>
#include <string>

enum class my_signals
{
  open,
  print,
  close
};

struct my_event_handler : public handlebars::handler<my_event_handler, my_signals, const std::string&>
{
  void open(const std::string& Name, const std::string& Msg)
  {
    std::cout << "Hello, " << Name << "!" << std::endl;
    std::cout << Msg << std::endl;
  }
  void print(const std::string& Msg) { std::cout << Msg << std::endl; }
  void close(const std::string& Name, const std::string& Msg)
  {
    std::cout << "Goodbye " << Name << "." << std::endl;
    std::cout << Msg << std::endl;
  }

  my_event_handler(const std::string& Name)
    : bound_name(Name)
  {
    connect_bind(my_signals::open, &my_event_handler::open, bound_name);
    connect(my_signals::print, &my_event_handler::print);
    connect_bind(my_signals::close, &my_event_handler::close, bound_name);
  }

private:
  const std::string bound_name;
};

int
main()
{
  using dispatcher = handlebars::dispatcher<my_signals, const std::string&>;
  auto steve = my_event_handler("Steve");
  auto hank = my_event_handler("Hank");
  dispatcher::push_event(my_signals::open, "How are you?");
  dispatcher::push_event(my_signals::print, "hmm...");
  dispatcher::push_event(my_signals::close, "See you later.");
  dispatcher::respond();
  return 0;
}