#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <boost/test/unit_test.hpp>
#pragma GCC diagnostic pop

#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/testing/tester.hpp>

#include <fc/exception/exception.hpp>
#include <fc/variant_object.hpp>

#include <contracts.hpp>

#include "eosio_system_tester.hpp"

/*
 * register test suite `ram_tests`
 */
BOOST_AUTO_TEST_SUITE(ram_tests)
#if 1
/*************************************************************************************
 * ram_tests test case
 *************************************************************************************/
BOOST_FIXTURE_TEST_CASE(ram_tests, eosio_system::eosio_system_tester) { try {
   auto init_request_bytes = 80000 + 7110; // `7110' is for table token row
   const auto increment_contract_bytes = 10000;
   const auto table_allocation_bytes = 12000;
   BOOST_REQUIRE_MESSAGE(table_allocation_bytes > increment_contract_bytes, "increment_contract_bytes must be less than table_allocation_bytes for this test setup to work");
   //1. gax have ram 70000
   buyrambytes(config::system_account_name, config::system_account_name, 70000);
   produce_blocks(10);
   //2. testram11111 have ram 80000 + 7110 + 40 = 87150
   create_account_with_resources("testram11111"_n,config::system_account_name, init_request_bytes + 40);
   //3. testram22222 have ram 80000 + 7110 + 1190 = 88300
   create_account_with_resources("testram22222"_n,config::system_account_name, init_request_bytes + 1190);
   produce_blocks(10);
   //4. stake CPU and net for testram11111
   BOOST_REQUIRE_EQUAL( success(), stake( name("gax.stake"), name("testram11111"), core_from_string("10.0000"), core_from_string("5.0000") ) );
   produce_blocks(10);
   //5.deploy ram_limit contract code to testram11111 if fail because of insufficient RAM, buy more RAM 
   //at most 87110 + 10*10000
   for (auto i = 0; i < 10; ++i) {
      try {
         set_code( "testram11111"_n, test_contracts::test_ram_limit_wasm() );
         break;
      } catch (const ram_usage_exceeded&) {
         init_request_bytes += increment_contract_bytes;
         buyrambytes(config::system_account_name, "testram11111"_n, increment_contract_bytes);
         buyrambytes(config::system_account_name, "testram22222"_n, increment_contract_bytes);
      }
   }
   produce_blocks(10);
   //6.deploy ram_limit contract abi to testram11111 if fail because of insufficient RAM, buy more RAM
   //at most 87110 + 10*10000 + 10*10000
   for (auto i = 0; i < 10; ++i) {
      try {
         set_abi( "testram11111"_n, test_contracts::test_ram_limit_abi().data() );
         break;
      } catch (const ram_usage_exceeded&) {
         init_request_bytes += increment_contract_bytes;
         buyrambytes(config::system_account_name, "testram11111"_n, increment_contract_bytes);
         buyrambytes(config::system_account_name, "testram22222"_n, increment_contract_bytes);
      }
   }
   produce_blocks(10);
   //testram11111 and testram22222 have equal quantity RAM
   //deploy ram_limit contract code and abi to testram22222
   set_code( "testram22222"_n, test_contracts::test_ram_limit_wasm() );
   set_abi( "testram22222"_n, test_contracts::test_ram_limit_abi().data() );
   produce_blocks(10);
   //query total staked RAM of testram11111 from table userres
   auto total = get_total_stake( "testram11111"_n );
   const auto init_bytes =  total["ram_bytes"].as_uint64();
   //query RAM consumed of testram11111
   chainbase::database& db = const_cast<database_manager&>(control->dbm()).main_db();
   auto rlm = control->get_resource_limits_manager();
   auto initial_ram_usage = rlm.get_account_ram_usage("testram11111"_n, db);
   /**
   *init_bytes: total staked ram.
   *table_allocation_bytes: system will consume to store data into database.
   *init_request_bytes: requested but be consumed to contract deploying.
   *more_ram: delta need to buy for testram11111
   */
   // calculate how many more bytes we need to have table_allocation_bytes for database stores
   auto more_ram = table_allocation_bytes + init_bytes - init_request_bytes;
   BOOST_REQUIRE_MESSAGE(more_ram >= 0, "Underlying understanding changed, need to reduce size of init_request_bytes");
   //initial_ram_usage: ram consumed.
   wdump((init_bytes)(initial_ram_usage)(init_request_bytes)(more_ram) );
   buyrambytes(config::system_account_name, "testram11111"_n, more_ram);
   buyrambytes(config::system_account_name, "testram22222"_n, more_ram);

   TESTER* tester = this;
   // allocate just under the allocated bytes
   // available = init_bytes + more_ram - initial_ram_usage = 19002
   // test content store table: only think about content self
   // | item index  | item size | available | expection
   // |    1        | 1780      | 19002     |  
   // |    2        | 1780      | x - 1780  |  
   // |    ......
   // |   10        | 1780      | x - 9*1780|  success
   tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                        ("payer", "testram11111")
                        ("from", 1)
                        ("to", 10)
                        ("size", 1780 /*1910*/));
   produce_blocks(1);
   auto ram_usage = rlm.get_account_ram_usage("testram11111"_n, db);

   total = get_total_stake( "testram11111"_n );
   const auto ram_bytes =  total["ram_bytes"].as_uint64();
   wdump((ram_bytes)(ram_usage)(initial_ram_usage)(init_bytes)(ram_usage - initial_ram_usage)(init_bytes - ram_usage) );

   wlog("ram_tests 1    %%%%%%");
   // allocate just beyond the allocated bytes
   //available = if index == 11 ? 19002 - 17800 = 1202 : 19002
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1920      | 19002     |  
   // |    2        | 1920      | x - 1920  |  
   // |    ......
   // |   10        | 1920      | x - 9*1920| ram_usage_exceeded
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                           ("payer", "testram11111")
                           ("from", 1)
                           ("to", 10)
                           ("size", /*1790*/ 1920)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   wlog("ram_tests 2    %%%%%%");
   produce_blocks(1);
   //if transaction fail, RAM usage isn't accumulation
   BOOST_REQUIRE_EQUAL(ram_usage, rlm.get_account_ram_usage("testram11111"_n, db));

   // update the entries with smaller allocations so that we can verify space is freed and new allocations can be made
   //available = if index == 11 ? 19002 - 17800 = 1202 : 19002
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1680      | 19002     |  
   // |    2        | 1680      | x - 1680  |  
   // |    ......
   // |   10        | 1680      | x - 9*1680| success
   tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                        ("payer", "testram11111")
                        ("from", 1)
                        ("to", 10)
                        ("size", 1680/*1810*/));
   produce_blocks(1);
   //(1780-1680)*10 = 1000
   BOOST_REQUIRE_EQUAL(ram_usage - 1000, rlm.get_account_ram_usage("testram11111"_n, db));

   // verify the added entry is beyond the allocation bytes limit
   //available = 19002 
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1810      | 19002     |  
   // |    2        | 1810      | x - 1810  |  
   // |    ......
   // |   10        | 1810      |           | 
   // |   11        | 1810      | x- 10*1810| ram_usage_exceeded
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                           ("payer", "testram11111")
                           ("from", 1)
                           ("to", 11)
                           ("size", /*1680*/1810)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);
   //if transaction fail, RAM usage isn't accumulation
   BOOST_REQUIRE_EQUAL(ram_usage - 1000, rlm.get_account_ram_usage("testram11111"_n, db));

   // verify the new entry's bytes minus the freed up bytes for existing entries still exceeds the allocation bytes limit
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1760      | 19002     |  
   // |    2        | 1760      | x - 1760  |  
   // |    ......
   // |   10        | 1760      |           | 
   // |   11        | 1760      | x- 10*1760| ram_usage_exceeded
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                           ("payer", "testram11111")
                           ("from", 1)
                           ("to", 11)
                           ("size", 1760)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL(ram_usage - 1000, rlm.get_account_ram_usage("testram11111"_n, db));

   // verify the new entry's bytes minus the freed up bytes for existing entries are under the allocation bytes limit
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1720      | 19002     |  
   // |    2        | 1720      | x - 1720  |  
   // |    ......
   // |   10        | 1720      |           | 
   // |   11        | 1720      | x- 10*1720| success
   //                           | 82        |
   tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                        ("payer", "testram11111")
                        ("from", 1)
                        ("to", 11)
                        ("size", /*1600*/1720));
   produce_blocks(1);
   ram_usage = rlm.get_account_ram_usage("testram11111"_n, db);
   //delete an item, free ram
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1720      | 19002     |  
   // |    3        | 0         | x - 0     |  
   // |    4        | 1720      | x - 1720  |
   // | ......
   // |   10        | 1720      |           | 
   // |   11        | 1720      | x- 9*1720 | success
   //                           | 1802      |
   tester->push_action( "testram11111"_n, "rmentry"_n, "testram11111"_n, mvo()
                        ("from", 3)
                        ("to", 3));
   produce_blocks(1);
   ram_usage = rlm.get_account_ram_usage("testram11111"_n, db);
   // verify that the new entry will exceed the allocation bytes limit
   // test content store table:
   // | item index  | item size | available | expection
   // |    1        | 1720      | 19002     |  
   // |    3        | 0         | x - 0     |  
   // |    4        | 1720      | x - 1720  |
   // | ......
   // |   10        | 1720      |           | 
   // |   11        | 1720      | x- 9*1720 | success
   // |   12        | 1780      | 1802      |
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                           ("payer", "testram11111")
                           ("from", 12)
                           ("to", 12)
                           ("size", 1780)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify that the new entry is under the allocation bytes limit
   tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                        ("payer", "testram11111")
                        ("from", 12)
                        ("to", 12)
                        ("size", /*1620*/1720));
   produce_blocks(1);

   // verify that anoth new entry will exceed the allocation bytes limit, to setup testing of new payer
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                           ("payer", "testram11111")
                           ("from", 13)
                           ("to", 13)
                           ("size", 1660)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   // verify that the new entry is under the allocation bytes limit
   tester->push_action( "testram11111"_n, "setentry"_n, {"testram11111"_n,"testram22222"_n}, mvo()
                        ("payer", "testram22222")
                        ("from", 12)
                        ("to", 12)
                        ("size", 1720));
   produce_blocks(1);

   // verify that another new entry that is too big will exceed the allocation bytes limit, to setup testing of new payer
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                           ("payer", "testram11111")
                           ("from", 13)
                           ("to", 13)
                           ("size", 1900)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram11111 has insufficient ram"));
   produce_blocks(1);

   wlog("ram_tests 18    %%%%%%");
   // verify that the new entry is under the allocation bytes limit, because entry 12 is now charged to testram22222
   tester->push_action( "testram11111"_n, "setentry"_n, "testram11111"_n, mvo()
                        ("payer", "testram11111")
                        ("from", 13)
                        ("to", 13)
                        ("size", 1720));
   produce_blocks(1);

   // verify that new entries for testram22222 exceed the allocation bytes limit
   // testram22222 available 20152
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, {"testram11111"_n,"testram22222"_n}, mvo()
                           ("payer", "testram22222")
                           ("from", 12)
                           ("to", 21)
                           ("size", /*1930*/2930)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram22222 has insufficient ram"));
   produce_blocks(1);

   // verify that new entries for testram22222 are under the allocation bytes limit
   tester->push_action( "testram11111"_n, "setentry"_n, {"testram11111"_n,"testram22222"_n}, mvo()
                        ("payer", "testram22222")
                        ("from", 12)
                        ("to", 21)
                        ("size", 1910));
   produce_blocks(1);

   // verify that new entry for testram22222 exceed the allocation bytes limit
   BOOST_REQUIRE_EXCEPTION(
      tester->push_action( "testram11111"_n, "setentry"_n, {"testram11111"_n,"testram22222"_n}, mvo()
                           ("payer", "testram22222")
                           ("from", 22)
                           ("to", 22)
                           ("size", 1910)),
                           ram_usage_exceeded,
                           fc_exception_message_starts_with("account testram22222 has insufficient ram"));
   produce_blocks(1);

   tester->push_action( "testram11111"_n, "rmentry"_n, "testram11111"_n, mvo()
                        ("from", 20)
                        ("to", 20));
   produce_blocks(1);

   // verify that new entry for testram22222 are under the allocation bytes limit
   tester->push_action( "testram11111"_n, "setentry"_n, {"testram11111"_n,"testram22222"_n}, mvo()
                        ("payer", "testram22222")
                        ("from", 22)
                        ("to", 22)
                        ("size", 1910));
   produce_blocks(1);

} FC_LOG_AND_RETHROW() }
#endif
BOOST_AUTO_TEST_SUITE_END()
