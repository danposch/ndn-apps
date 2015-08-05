#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include "../utils/OptionPrinter.hpp"

using namespace boost::program_options;

namespace ndn
{

class Producer : noncopyable
{
public:

  Producer(std::string prefix, int data_size, int fresshness_seconds)
  {
    this->prefix = prefix;
    this->data_size = data_size;
    this->fresshness_seconds = fresshness_seconds;
  }

  void run()
  {
    m_face.setInterestFilter(this->prefix,
                             bind(&Producer::onInterest, this, _1, _2),
                             RegisterPrefixSuccessCallback(),
                             bind(&Producer::onRegisterFailed, this, _1, _2));
    m_face.processEvents();
  }

private:

  std::string generateContent(const int length)
  {
    static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

    std::string content;

    for (int i = 0; i < length; ++i)
      content += alphanum[rand() % (sizeof(alphanum) - 1)];
    return content;
  }

  void onInterest(const InterestFilter& filter, const Interest& interest)
  {
    std::cout << "<< I: " << interest << std::endl;

    // Create new name, based on Interest's name
    Name dataName(interest.getName());
    dataName.appendVersion();  // add "version" component (current UNIX timestamp in milliseconds)

    std::string content = generateContent(data_size);

    // Create Data packet
    shared_ptr<Data> data = make_shared<Data>();
    data->setName(dataName);
    data->setFreshnessPeriod(time::seconds(fresshness_seconds));
    data->setContent(reinterpret_cast<const uint8_t*>(content.c_str()), content.size());

    // Sign Data packet with default identity
    m_keyChain.sign(*data);

    // Return Data packet
    m_face.put(*data);
  }

  void onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \""
              << prefix << "\" in local hub's daemon (" << reason << ")"
              << std::endl;
    m_face.shutdown();
  }

private:
  Face m_face;
  KeyChain m_keyChain;
  int data_size;
  int fresshness_seconds;
  std::string prefix;
};

} // namespace ndn

int main(int argc, char** argv)
{
  std::string appName = boost::filesystem::basename(argv[0]);

  options_description desc("Programm Options");
  desc.add_options ()
      ("help,h", "Prints help.")
      ("prefix,p", value<std::string>()->required (), "Prefix the Producer listens too.")
      ("data-size,s", value<int>()->required (), "The size of the datapacket in bytes.")
      ("freshness-time,f", value<int>()->required (), "Freshness time of the content in seconds. (Default 5min)")
      ;

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

  ndn::Producer producer(vm["prefix"].as<std::string>(),
                         vm["data-size"].as<int>(),
                         vm["freshness-time"].as<int>());
  try
  {
    producer.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }

  return 0;
}
