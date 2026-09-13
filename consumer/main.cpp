// Composition root of the consumer: parses options, wires concrete
// implementations to the interfaces ConsumerApp depends on, and runs it.
#include <unistd.h>

#include <exception>
#include <iostream>
#include <memory>

#include "consumer_app.hpp"
#include "consumer_options.hpp"
#include "consumer_reporter.hpp"
#include "packet_validator.hpp"
#include "pause_policy.hpp"
#include "pc/checksum.hpp"
#include "pc/cli.hpp"
#include "pc/control.hpp"
#include "pc/keyboard_source.hpp"
#include "pc/shm_transport.hpp"
#include "pc/signal_source.hpp"

int main(int argc, char* argv[]) {
    using namespace pc;
    try {
        const auto options = parse_consumer_options(argc, argv, std::cout);
        if (!options) {
            return 0;
        }

        RunControl control;
        SignalControlSource signals(control);  // first: later threads inherit its signal mask
        std::unique_ptr<IControlSource> keyboard;
        if (options->keyboard && KeyboardControlSource::available(STDIN_FILENO)) {
            keyboard = std::make_unique<KeyboardControlSource>(control, STDIN_FILENO);
        }

        const Crc32 checksum;
        PacketValidator validator(checksum);
        BlockingPausePolicy policy;
        ShmPacketSource source(options->shm_name);
        ConsoleConsumerReporter reporter(std::cout);

        ConsumerConfig config;
        config.report_interval = options->report_interval;
        config.max_defect_logs = options->max_defect_logs;
        config.once = options->once;

        reporter.info(keyboard ? "Keyboard: any key = pause/resume, q = quit"
                               : "Keyboard: not available (stdin is not an interactive terminal)");
        ConsumerApp app(config, source, validator, policy, control, reporter);
        return app.run();
    } catch (const UsageError& error) {
        std::cerr << "consumer: " << error.what() << "\nTry 'consumer --help' for usage.\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "consumer: fatal: " << error.what() << '\n';
        return 2;
    }
}
