#pragma once
#include <fc/log/appender.hpp>
#include <fc/reflect/reflect.hpp>

namespace fc {

   /**
    * Specialized appender for deep mind tracer that sends log messages
    * through `stdout` correctly formatted for latter consumption by
    * deep mind postprocessing tools from dfuse.
    */
   class dmlog_appender : public appender
   {
       public:
            struct config
            {
               std::string file = "-";
            };
            explicit dmlog_appender( const variant& args );
            explicit dmlog_appender( const std::optional<config>& args) ;

            virtual ~dmlog_appender();
            virtual void initialize() override;

            virtual void log( const log_message& m ) override;

       private:
            dmlog_appender();
            class impl;
            std::unique_ptr<impl> my;
   };
}

FC_REFLECT(fc::dmlog_appender::config, (file))
