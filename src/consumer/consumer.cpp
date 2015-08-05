#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include "../utils/OptionPrinter.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/asio/deadline_timer.hpp"

using namespace boost::program_options;

namespace ndn {

class Consumer : noncopyable
{
public:

  Consumer(std::string prefix, int rate, int run_time) : m_face(m_ioService), m_scheduler(m_ioService)
  {
    this->prefix = prefix;
    this->rate = rate;
    this->counter = 0;
    this->run_time = run_time;
    this->stop_consumer = false;
  }

  void run()
  {
    boost::asio::deadline_timer timer(m_ioService, boost::posix_time::microseconds(1000000/rate));
    timer.async_wait(bind(&Consumer::expressInterest, this, &timer));

    boost::asio::deadline_timer stopTimer(m_ioService, boost::posix_time::seconds(run_time));
    stopTimer.async_wait(bind(&Consumer::stopConsumer, this));

    m_face.processEvents();
  }

private:

  void expressInterest(boost::asio::deadline_timer* timer)
  {
    Interest interest(Name(prefix + "/" + boost::lexical_cast<std::string>(counter++)));
    interest.setInterestLifetime(time::milliseconds(1000));
    interest.setMustBeFresh(true);

    m_face.expressInterest(interest,
                           bind(&Consumer::onData, this,  _1, _2),
                           bind(&Consumer::onTimeout, this, _1));

    std::cout << "Sending " << interest << std::endl;

    if(!stop_consumer)
    {
      timer->expires_at (timer->expires_at ()+ boost::posix_time::microseconds(1000000/rate));
      timer->async_wait(bind(&Consumer::expressInterest, this, timer));
    }
  }

  void onData(const Interest& interest, const Data& data)
  {
    std::cout << data << std::endl;
  }

  void onTimeout(const Interest& interest)
  {
    std::cout << "Timeout " << interest << std::endl;
  }

  void stopConsumer()
  {
    this->stop_consumer = true;
  }

private:
  boost::asio::io_service m_ioService;
  Face m_face;
  Scheduler m_scheduler;
  std::string prefix;
  int rate;
  int counter;
  int run_time;
  bool stop_consumer;
};

}

int
main(int argc, char** argv)
{

  std::string appName = boost::filesystem::basename(argv[0]);

  options_description desc("Programm Options");
  desc.add_options ()
      ("help,h", "Prints help.")
      ("prefix,p", value<std::string>()->required (), "Prefix the Consumer uses to request content.")
      ("rate,r", value<int>()->required (), "Interests per second issued.")
      ("run-time,t", value<int>()->required (), "Runtime of Producer in Seconds.");

  positional_options_description positionalOptions;
  variables_map vm;

  try
  {
    store(command_line_parser(argc, argv).options(desc)
                .positional(positionalOptions).run(),
              vm); // throws on error

    if ( vm.count("help")  )
    {

      rad::OptionPrinter::printStandardAppDesc(appName,
                                               std::cout,
                                               desc,
                                               &positionalOptions);
      return 0;
    }
    notify(vm); //notify if required parameters are not provided.
  }
  catch(boost::program_options::required_option& e)
  {
    rad::OptionPrinter::formatRequiredOptionError(e);
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    rad::OptionPrinter::printStandardAppDesc(appName,
                                             std::cout,
                                             desc,
                                             &positionalOptions);
    return -1;
  }
  catch(boost::program_options::error& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    rad::OptionPrinter::printStandardAppDesc(appName,
                                             std::cout,
                                             desc,
                                             &positionalOptions);
    return -1;
  }
  catch(std::exception& e)
  {
    std::cerr << "Unhandled Exception reached the top of main: "
              << e.what() << ", application will now exit" << std::endl;
    return -1;
  }

  ndn::Consumer consumer(vm["prefix"].as<std::string>(),
                         vm["rate"].as<int>(),
                         vm["run-time"].as<int>());

  try
  {
    consumer.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }
  return 0;
}
