#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <bitset>
#include <string>
#include <tuple>
#include <random>

#include "convolution.h"
#include "modem.h"
#include "util.h"

using namespace std;

// WAVE PCM soundfile format
typedef struct header_file
{
    char chunk_id[4];
    int chunk_size;
    char format[4];
    char subchunk1_id[4];
    int subchunk1_size;
    short int audio_format;
    short int num_channels;
    int sample_rate;
    int byte_rate;
    short int block_align;
    short int bits_per_sample;
    char subchunk2_id[4];
    int subchun2_size;
} header;

typedef struct header_file* header_p;

int main(int argc,const char* argv[])
{
	if (argc == 4){
		string modu_type = string(argv[1]);
		double Eb = atof(argv[2]);
		double N0 = atof(argv[3]);
		//N0 = modu_type.compare("BPSK")?N0:2*N0; //BPSK only one dimension noise
		string out_file_name = string("output/") + modu_type + string(argv[2]) + string(".wav");
		FILE * infile = fopen("input.wav","rb");
		FILE * outfile = fopen(out_file_name.c_str(),"wb");


		default_random_engine generator;
		normal_distribution<double> dist(0,sqrt(N0/2));

		bool is2D = modu_type.compare("BPSK");
		int count = 0;
		unsigned char rx_buff8[BUFSIZE],tx_buff8[BUFSIZE];
		header_p meta = (header_p)malloc(sizeof(header));
		int nb;
		
		size_t num_diff_packets = 0;
		size_t err_square_sum = 0;
		Convolution conver;
		Modem modemer(modu_type,Eb);

		if (infile)
		{
			fread(meta, 1, sizeof(header), infile);
			fwrite(meta,1, sizeof(*meta), outfile);
			while (!feof(infile))
			{
				nb = fread(tx_buff8,1,BUFSIZE,infile);
				count++;

				//collect the packet (120 bits)
				string data_str;
				for(size_t i=0;i<nb;++i){
					bitset<8> tmp(tx_buff8[i]);
					data_str += tmp.to_string();
				}

				//zero-padding
				for(size_t i=0;i<BUFSIZE-nb;++i){
					data_str += "00000000";
				}
				bitset<8*BUFSIZE> tx_info_bits(data_str);

				bitset<8*BUFSIZE*2> tx_coded_bits = conver.encode(tx_info_bits);
				
				vector<tuple<double,double> > tx_symbols;
				tx_symbols = modemer.modulation(tx_coded_bits);

				vector<tuple<double,double> > rx_symbols;
				rx_symbols.resize(tx_symbols.size());

				//add noise
				for(size_t i=0;i<tx_symbols.size();++i){
					double x,y;
					tie(x,y) = tx_symbols[i];
					if(is2D){
						x += dist(generator);
						y += dist(generator);
					}
					else{
						x += dist(generator);
					}
					rx_symbols[i] = make_tuple(x,y);
				}

				bitset<8*BUFSIZE*2> rx_coded_bits= modemer.demodulation(rx_symbols);

				bitset<8*BUFSIZE> rx_info_bits = conver.decode(rx_coded_bits);
				if(rx_info_bits != tx_info_bits) ++num_diff_packets;
				

				string rx_data_str = rx_info_bits.to_string();
				for(size_t i=0;i<nb;++i){
					bitset<8> tmp(rx_data_str.substr(8*i,8));
					rx_buff8[i] = static_cast<unsigned char>(tmp.to_ulong());
					err_square_sum += (rx_buff8[i] - tx_buff8[i]) * (rx_buff8[i] - tx_buff8[i]);
				}
				fwrite(rx_buff8,1,nb,outfile);
			}
			cout << "=====STATISTICS=====" << endl;
			cout << "N0 : " << N0 <<endl;
			cout << "Eb : " << Eb << endl;
			cout << " Number of frames in the input wave file are " << count << endl;
			cout << err_square_sum << endl;
		}
		return 0;
	}
	else{
		cerr << "Usage : ./audio <MODEM> <Eb> <N0>" << endl;
		exit(1);
	}
}
