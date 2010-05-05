
#include <aio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils.hpp"
#include "io_engine.hpp"
#include "workload.hpp"

int io_engine_t::contribute_open_flags() {
    if(config->operation == op_read)
        return O_RDONLY;
    else if(config->operation == op_write)
        return O_WRONLY;
    else
        check("Invalid operation", 1);
    
}

void io_engine_t::post_open_setup() {
}

void io_engine_t::pre_close_teardown() {
}

void io_engine_t::run_benchmark() {
    rnd_gen_t rnd_gen;
    char *buf;

    int res = posix_memalign((void**)&buf,
                             std::max(getpagesize(), config->block_size),
                             config->block_size);
    check("Error allocating memory", res != 0);

    rnd_gen = init_rnd_gen();
    check("Error initializing random numbers", rnd_gen == NULL);

    char sum = 0;
    while(!(*is_done)) {
        long long _ops = __sync_fetch_and_add(&ops, 1);
        
        ticks_t op_start_time = get_ticks();
        res = perform_op(buf, _ops, rnd_gen);
        ticks_t op_end_time = get_ticks();
        float op_ms = ticks_to_ms(op_end_time - op_start_time);
        if(op_ms > 5000.0f) {
            printf("start: %llu, end: %llu\n", op_start_time, op_end_time);
            op_ms = 5.0f;
        }

        // compute some status
        if(op_ms < min_op_time_in_ms)
            min_op_time_in_ms = op_ms;
        if(op_ms > max_op_time_in_ms)
            max_op_time_in_ms = op_ms;
        op_total_ms += op_ms;
        
        if(!res) {
            *is_done = 1;
            goto done;
        }
        // Read from the buffer to make sure there is no optimization
        // shenanigans
	sum += buf[0];
    }

done:
    free_rnd_gen(rnd_gen);
    free(buf);
}

int io_engine_t::perform_op(char *buf, long long ops, rnd_gen_t rnd_gen) {
    off64_t res;
    off64_t offset = prepare_offset(ops, rnd_gen, config);
    if(::is_done(offset, config)) {
        return 0;
    }

    if(config->duration_unit == dut_interactive) {
        char in;
        // Ask for confirmation before the operation
        printf("%lld: Press enter to perform operation, or 'q' to quit: ", ops);
        in = getchar();
        if(in == EOF || in == 'q') {
            return 0;
        }
    }
    
    // Perform the operation
    if(config->operation == op_read)
        perform_read_op(offset, buf);
    else if(config->operation == op_write)
        perform_write_op(offset, buf);
    
    return 1;
}

void io_engine_t::copy_io_state(io_engine_t *io_engine) {
    fd = io_engine->fd;
}

#include "io_engines.hpp"

io_engine_t* make_engine(io_type_t engine_type) {
    switch(engine_type) {
    case iot_stateful:
        return new io_engine_stateful_t();
        break;
    case iot_stateless:
        return new io_engine_stateless_t();
        break;
    case iot_paio:
        return new io_engine_paio_t();
        break;
    case iot_naio:
        return new io_engine_naio_t();
        break;
    case iot_mmap:
        return new io_engine_mmap_t();
        break;
    default:
        check("Unknown engine type", 1);
    }
    return NULL;
}
