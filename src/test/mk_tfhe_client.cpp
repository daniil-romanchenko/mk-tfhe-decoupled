//#include <stdio.h>
#include <iostream>
//#include <iomanip>
#include <cstdlib>
//#include <cmath>
#include <fstream>
#include <sys/time.h>
#include "tfhe.h"
#include "polynomials.h"
//#include "lwesamples.h"
//#include "lwekey.h"
#include "lweparams.h"
#include "tlwe.h"
//#include "tgsw.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "mkTFHEparams.h"
#include "mkTFHEkeys.h"
#include "mkTFHEkeygen.h"
#include "mkTFHEsamples.h"
#include "mkTFHEfunctions.h"





 

using namespace std;

// **********************************************************************************
// **************************** STATIC PARAMETERS ***********************************
// **********************************************************************************

// generate params
static const int32_t k = 1;
static const double ks_stdev = 3.05e-5;// 2.44e-5; //standard deviation
static const double bk_stdev = 3.72e-9; // 3.29e-10; //standard deviation
static const double max_stdev = 0.012467; //max standard deviation for a 1/4 msg space
static const int32_t n = 560; //500;            // LWE modulus
static const int32_t n_extract = 1024;    // LWE extract modulus (used in bootstrapping)
//    static const int32_t hLWE = 0;         // HW secret key LWE --> not used
static const double stdevLWE = 0.012467;      // LWE ciphertexts standard deviation
static const int32_t Bksbit = 2;       // Base bit key switching
static const int32_t dks = 8;          // dimension key switching
static const double stdevKS = ks_stdev; // 2.44e-5;       // KS key standard deviation
static const int32_t N = 1024;            // RLWE,RGSW modulus
static const int32_t hRLWE = 0;        // HW secret key RLWE,RGSW --> not used
static const double stdevRLWEkey = bk_stdev; // 3.29e-10; // 0; // 0.012467;  // RLWE key standard deviation
static const double stdevRLWE = bk_stdev; // 3.29e-10; // 0; // 0.012467;     // RLWE ciphertexts standard deviation
static const double stdevRGSW = bk_stdev; // 3.29e-10;     // RGSW ciphertexts standard deviation
static const int32_t Bgbit = 8;        // Base bit gadget
static const int32_t dg = 4;           // dimension gadget
static const double stdevBK = bk_stdev; // 3.29e-10;       // BK standard deviation
static const int32_t parties = 4;      // number of parties

// new parameters
// 2 parties, B=2^9, d=3 -> works
// 4 parties, B=2^8, d=4 -> works
// 8 parties, B=2^6, d=5 -> works



// **********************************************************************************
// ********************************* MAIN *******************************************
// **********************************************************************************


void dieDramatically(string message) {
    cerr << message << endl;
    abort();
}

static void gen_keys(int ID)
{
    cout << "Create keys folder" << endl;
    mkdir("./keys", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    // Key generation
    cout << "Starting KEY GENERATION" << endl;
    //TODO: set random generator

    LweParams *extractedLWEparams = new_LweParams(n_extract, ks_stdev, max_stdev);
    LweParams *LWEparams = new_LweParams(n, ks_stdev, max_stdev);
    TLweParams *RLWEparams = new_TLweParams(N, k, bk_stdev, max_stdev);
    MKTFHEParams *MKparams = new_MKTFHEParams(n, n_extract, 0, stdevLWE, Bksbit, dks, stdevKS, N,
                                              hRLWE, stdevRLWEkey, stdevRLWE, stdevRGSW, Bgbit, dg, stdevBK, parties);
    // load common key
    MKRLweKey* dPK = new_MKRLweKey(RLWEparams, MKparams);
    //TODO: error handlingd
    dPK->deserialize("keys/CommonKey.binary");

    MKparams->hLWE = ID;
    cout << "Reading \"CommonKey.binary\": DONE!" << endl;

    // LWE key
    MKLweKey* MKlwekey = new_MKLweKey(LWEparams, MKparams);
    MKLweKeyGenSingle(MKlwekey);
    MKlwekey->serialize("keys/Secret.binary");
    cout << "KeyGen MKlwekey: DONE!" << endl;

    // RLWE key
    MKRLweKey* MKrlwekey = new_MKRLweKey(RLWEparams, MKparams);
    MKRLweKeyGenSingle(MKrlwekey, dPK);
    //        Try serialization
    MKrlwekey->serialize("keys/Public.binary");

    cout << "KeyGen MKRlwekey: DONE!" << endl;

    // LWE key extracted
    MKLweKey* MKextractedlwekey = new_MKLweKey(extractedLWEparams, MKparams);
    MKtLweExtractKeySingle(MKextractedlwekey, MKrlwekey);
    cout << "Extract MKextractedlwekey: DONE!" << endl;

    // bootstrapping + key switching keys
    MKLweBootstrappingKey_v2* MKlweBK = new_MKLweBootstrappingKey_v2(LWEparams, RLWEparams, MKparams);
    MKlweCreateBootstrappingKey_v2Single(MKlweBK, MKlwekey, MKrlwekey, MKextractedlwekey,
                                         extractedLWEparams, LWEparams, RLWEparams, MKparams);
    //        Try serialization
    MKlweBK->serialize("keys/KSKBSK.binary");

    cout << "KeyGen MKlweBK: DONE!" << endl;
    cout << "Finished KEY GENERATION" << endl;

    delete_MKLweBootstrappingKey_v2(MKlweBK);
    delete_MKLweKey(MKextractedlwekey);
    delete_MKRLweKey(MKrlwekey);
    delete_MKLweKey(MKlwekey);
    // delete params
    delete_MKTFHEParams(MKparams);
    delete_TLweParams(RLWEparams);
    delete_LweParams(LWEparams);
    delete_LweParams(extractedLWEparams);

}

static void first_enc_bit(int32_t BIT)
{
    // Key generation
    cout << "Starting BIT ENCRYPTION" << endl;

//    LweParams *extractedLWEparams = new_LweParams(n_extract, ks_stdev, max_stdev);
    LweParams *LWEparams = new_LweParams(n, ks_stdev, max_stdev);
    TLweParams *RLWEparams = new_TLweParams(N, k, bk_stdev, max_stdev);
    MKTFHEParams *MKparams = new_MKTFHEParams(n, n_extract, 0, stdevLWE, Bksbit, dks, stdevKS, N,
                                              hRLWE, stdevRLWEkey, stdevRLWE, stdevRGSW, Bgbit, dg, stdevBK, parties);

    // LWE key
    MKLweKey* MKlwekey = new_MKLweKey(LWEparams, MKparams);
    //        Try deserialization
    MKlwekey->deserialize("keys/Secret.binary");
    cout << "Reading MKlwekey: DONE!" << endl;

    MKLweSample *sample = new_MKLweSample(LWEparams, MKparams);

    MKbootsSymEncryptSingleFirst(sample, BIT, MKlwekey);

    //        Try serialization
    {
        char buffer [50];
        sprintf (buffer, "sample%d.binary", MKlwekey->MKparams->hLWE + 1);
        sample->serialize(buffer);
    }
    cout << "First encryption: DONE!" << endl;
    cout << "Starting BIT ENCRYPTION" << endl;

    delete_MKLweKey(MKlwekey);
    // delete params
    delete_MKTFHEParams(MKparams);
    delete_TLweParams(RLWEparams);
    delete_LweParams(LWEparams);
}

static void next_enc_bit(string path)
{
    // Key generation
    cout << "Starting BIT ENCRYPTION" << endl;

//    LweParams *extractedLWEparams = new_LweParams(n_extract, ks_stdev, max_stdev);
    LweParams *LWEparams = new_LweParams(n, ks_stdev, max_stdev);
    TLweParams *RLWEparams = new_TLweParams(N, k, bk_stdev, max_stdev);
    MKTFHEParams *MKparams = new_MKTFHEParams(n, n_extract, 0, stdevLWE, Bksbit, dks, stdevKS, N,
                                              hRLWE, stdevRLWEkey, stdevRLWE, stdevRGSW, Bgbit, dg, stdevBK, parties);

    // LWE key
    MKLweKey* MKlwekey = new_MKLweKey(LWEparams, MKparams);
    //        Try deserialization
    MKlwekey->deserialize("keys/Secret.binary");
    cout << "Reading MKlwekey: DONE!" << endl;

    MKLweSample *sample = new_MKLweSample(LWEparams, MKparams);
    //        Try deserialization
    sample->deserialize(path);
    MKbootsSymEncryptSingle(sample, MKlwekey);
    //        Try serialization
    sample->serialize(path);

    cout << "First encryption: DONE!" << endl;
    cout << "Starting BIT ENCRYPTION" << endl;

    delete_MKLweKey(MKlwekey);
    // delete params
    delete_MKTFHEParams(MKparams);
    delete_TLweParams(RLWEparams);
    delete_LweParams(LWEparams);
}

static void show_usage(string name)
{
    cerr << "Usage: " << name << " OPTION\n"
         << "Options:\n"
         << "\tg (1 | 2 | 3 | 4)\tGenerate keys for party with ID - Secret key, Public key, Bootstrapping key, KeySwitching key" << endl
         << "\te (0 | 1)\tEncrypt bit and save it" << endl
         << endl;
}

int32_t main(int argc, char* argv[]) {
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }
    vector <string> sources;
    string destination;

    switch (*argv[1])
    {
        default: {
            printf("Unknown option -%c\n\n", (*argv)[1]);
            break;
        }
        case 'e': {
            string BIT = argv[2];
            if (BIT == "0")
                first_enc_bit(0);
            else
                first_enc_bit(1);
            break;
        }
        case 'n': {
            next_enc_bit(argv[2]);
            break;
        }
        case 'g': {
            string ID = argv[2];
            if (ID == "1") {
                gen_keys(0);
            } else if (ID == "2") {
                gen_keys(1);
            } else if (ID == "3") {
                gen_keys(2);
            } else if (ID == "4") {
                gen_keys(3);
            } else {
                cout << endl << "IDs 1-4 are supported only" << endl;
                return 1;
            }
            return 0;
            break;
        }
    }


    return 0;
}
