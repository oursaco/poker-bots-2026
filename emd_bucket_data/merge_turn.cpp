#include "external/omp/HandEvaluator.h"
#include "external/omp/Hand.h"
#include "bucket_gen/river_encoding.h"

using namespace omp;
using namespace std;


void saveEquity(vector<float> v1, vector<float> v2,string dir)
{
    ofstream ouf(dir, ios::binary);
    for(int i=0; i<v1.size(); i++)
        ouf.write(reinterpret_cast<const char *>(&v1[i]), sizeof(float));
    for(int i=0; i<v2.size(); i++)
        ouf.write(reinterpret_cast<const char *>(&v2[i]), sizeof(float));
    ouf.close();
}

int main(){

    int encoding_index = 161057;
    std::ifstream eq1("emd_bucket_data/turn_equity_1.bin", std::ios::binary);
    std::ifstream eq2("emd_bucket_data/turn_equity_2.bin", std::ios::binary);
    std::vector<float> equity1(100 * encoding_index);
    eq1.read(reinterpret_cast<char*>(equity1.data()), equity1.size() * sizeof(float));
    std::vector<float> equity2(69 * encoding_index);
    eq2.read(reinterpret_cast<char*>(equity2.data()), equity2.size() * sizeof(float));

    saveEquity(equity1, equity2, "emd_bucket_data/turn_equity.bin");
}