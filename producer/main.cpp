// Composition root of the producer: parses options, wires concrete
// implementations to the interfaces ProducerApp depends on, and runs it.
#include <unistd.h>

#include <exception>
#include <iostream>
#include <memory>

#include "packet_builder.hpp"
#include "payload_generator.hpp"
#include "pc/checksum.hpp"
#include "pc/cli.hpp"
#include "pc/control.hpp"
#include "pc/keyboard_source.hpp"
#include "pc/shm_transport.hpp"
#include "pc/signal_source.hpp"
#include "producer_app.hpp"
#include "producer_options.hpp"
#include "producer_reporter.hpp"

int main(int argc, char* argv[]) {
    using namespace pc;
    try {
        const auto options = parse_producer_options(argc, argv, std::cout);
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
        const auto generator = make_payload_generator(options->generator, options->seed);
        PacketBuilder builder(*generator, checksum);
        ShmPacketSink sink(options->shm_name, options->ring_size);
        ConsoleProducerReporter reporter(std::cout);

        ProducerConfig config;
        config.payload_size = options->payload_size;
        config.max_packets = options->count;
        config.report_interval = options->report_interval;

        reporter.info(keyboard ? "Keyboard: any key = pause/resume, q = quit"
                               : "Keyboard: not available (stdin is not an interactive terminal)");
        ProducerApp app(config, sink, builder, control, reporter);
        return app.run();
    } catch (const UsageError& error) {
        std::cerr << "producer: " << error.what() << "\nTry 'producer --help' for usage.\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "producer: fatal: " << error.what() << '\n';
        return 2;
    }
}
