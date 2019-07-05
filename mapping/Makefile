test: raw_test.c
	gcc -o raw_test raw_test.c

init: init.c
	gcc -o init -D_GNU_SOURCE init.c


chunk: identify_chunk.c
	gcc -o identify_chunk -D_GNU_SOURCE identify_chunk.c

interleave: identify_interleave.cpp
	g++ -std=c++11 -o identify_interleave -D_GNU_SOURCE identify_interleave.cpp -lpthread

mapping: identify_mapping.cpp
	g++ -std=c++11 -o identify_mapping -D_GNU_SOURCE identify_mapping.cpp -lpthread

split_request: split_request.cpp
	g++ -std=c++11 -o split_request -D_GNU_SOURCE split_request.cpp -lpthread

shear_pattern: shear_identify_pattern.cpp
	g++ -std=c++11 -o shear_pattern -D_GNU_SOURCE shear_identify_pattern.cpp -lpthread

random_overwrite: random_overwrite.cpp
	g++ -std=c++11 -o random_overwrite -D_GNU_SOURCE random_overwrite.cpp -lpthread

multi_thread_aio: multi_thread_aio.cpp ConsumerProducerQueue.h
	g++ -std=c++11 -o multi_thread_aio -D_GNU_SOURCE multi_thread_aio.cpp ConsumerProducerQueue.h -lpthread -laio

chunk_multi_thread_aio: multi_thread_aio_for_chunk.cpp ConsumerProducerQueue.h
	g++ -std=c++11 -o chunk_multi_thread_aio -D_GNU_SOURCE multi_thread_aio_for_chunk.cpp ConsumerProducerQueue.h -lpthread -laio

pair_read: pair_read.cpp
	g++ -std=c++11 -o pair_read -D_GNU_SOURCE pair_read.cpp -lpthread

clean:
	rm init raw_test identify_chunk identify_interleave identify_mapping
