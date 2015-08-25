#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include "../utils/OptionPrinter.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/asio/deadline_timer.hpp"

#include <vector>
#include <cstdio>
#include <iostream>
#include <fstream>

using namespace boost::program_options;

namespace ndn {

class Consumer : noncopyable
{
public:

  Consumer(std::string prefix, int rate, int run_time, int i_lifetime) : m_face(m_ioService), m_scheduler(m_ioService)
  {
    this->prefix = prefix;
    this->rate = rate;
    this->counter = 0;
    this->run_time = run_time;
    this->lifetime = i_lifetime;
    this->stop_consumer = false;

    this->data_received = 0;
    this->interest_send = 0;
    this-> rtx_counter = 0;
    this->debug = false;
    this->rtx = false;
  }

  void run()
  {
    boost::asio::deadline_timer timer(m_ioService, boost::posix_time::microseconds(1000000/rate));
    timer.async_wait(bind(&Consumer::expressInterest, this, &timer));

    boost::asio::deadline_timer stopTimer(m_ioService, boost::posix_time::seconds(run_time));
    stopTimer.async_wait(bind(&Consumer::stopConsumer, this));

    m_face.processEvents();

    double ratio = ((double) data_received) / (double) interest_send;

    std::cout << "Distinguished Interests Send: " << interest_send << std::endl;
    if(rtx)
      std::cout << "Retransmissions: " << rtx_counter << std::endl;
    else
      std::cout << "Retransmissions: Disabled" << std::endl;
    std::cout << "Total Interests Send: " << interest_send + rtx_counter << std::endl;
    std::cout << "Distinguished Interests Satisfied: " << data_received << std::endl;
    std::cout << "Total Interset/Data ratio: " << ratio << std::endl;
  }

  void setDebug(bool debug)
  {
    this->debug = debug;
  }

  void setRtx(bool rtx)
  {
    this->rtx = rtx;
  }

private:

  void expressInterest(boost::asio::deadline_timer* timer)
  {
    if(stop_consumer)
      return;

    if(rtx_queue.size () > 0)
    {
      onRetransmission();
    }
    else // new interest
    {
      Interest interest(Name(prefix + "/" + boost::lexical_cast<std::string>(counter++)));
      interest.setInterestLifetime(time::milliseconds(lifetime));
      interest.setMustBeFresh(true);

      m_face.expressInterest(interest,
                             bind(&Consumer::onData, this,  _1, _2),
                             bind(&Consumer::onTimeout, this, _1));

      if(debug)
        std::cout << "Sending: " << interest << std::endl;
      this->interest_send++;
    }

    timer->expires_at (timer->expires_at ()+ boost::posix_time::microseconds(1000000/rate));
    timer->async_wait(bind(&Consumer::expressInterest, this, timer));
  }

  void onData(const Interest& interest, const Data& data)
  {
    if(debug)
      std::cout << "Received: " << data << std::endl;
    this->data_received++;
  }

  void onTimeout(const Interest& interest)
  {
    if(debug)
      std::cout << "Timeout " << interest << std::endl;

    if(rtx)
      rtx_queue.push_back (interest.getName ().toUri ());
  }

  void onRetransmission()
  {

    Interest rtx_interest(Name(rtx_queue.front ()));
    rtx_queue.erase (rtx_queue.begin ());

    rtx_interest.setInterestLifetime(time::milliseconds(lifetime));
    rtx_interest.setMustBeFresh(true);

    m_face.expressInterest(rtx_interest,
                           bind(&Consumer::onData, this,  _1, _2),
                           bind(&Consumer::onTimeout, this, _1));

    if(debug)
      std::cout << "Rtx: " << rtx_interest << std::endl;

    rtx_counter++;
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
  int lifetime;
  bool stop_consumer;
  bool debug;
  bool rtx;

  unsigned int interest_send;
  unsigned int data_received;
  unsigned int rtx_counter;

  std::vector<std::string> rtx_queue;
};

}

int
main(int argc, char** argv)
{

  std::string appName = boost::filesystem::basename(argv[0]);

  options_description desc("Programm Options");
  desc.add_options ()
      ("help,h", "Prints help.")
      ("prefix,p", value<std::string>()->required (), "Prefix the Consumer uses to request content. (Required)")
      ("rate,r", value<int>()->required (), "Interests per second issued. (Required)")
      ("run-time,t", value<int>()->required (), "Runtime of Producer in Seconds. (Required)")
      ("rtx,x", "Enable Retransmissions. (Optional)")
      ("lifetime,l", value<int>(), "Interest Lifetime (Default 1000msec)")
      ("debug,v", "Enables Debug. (Optional)")
      ("logfile,o", value<std::string>(), "Writes Output to LogFile. (Optional)");

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

  std::streambuf *backup = NULL;
  std::ofstream output;
  if(vm.count ("logfile"))
  {
    std::string fname = vm["logfile"].as<std::string>();
    output.open (fname.c_str ());
    backup = std::cout.rdbuf();
    std::cout.rdbuf(output.rdbuf());
  }

  int lifetime = 1000;
  if(vm.count ("lifetime"))
  {
    lifetime = vm["lifetime"].as<int>();
  }

  ndn::Consumer consumer(vm["prefix"].as<std::string>(),
                         vm["rate"].as<int>(),
                         vm["run-time"].as<int>(),
                         lifetime);

  if(vm.count("debug"))
    consumer.setDebug (true);
  else
    consumer.setDebug (false);

  if(vm.count("rtx"))
    consumer.setRtx(true);
  else
    consumer.setRtx(false);

  try
  {
    consumer.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }

  if(vm.count ("logfile"))
  {
    std::cout.rdbuf(backup);
    output.close ();
  }

  return 0;
}
