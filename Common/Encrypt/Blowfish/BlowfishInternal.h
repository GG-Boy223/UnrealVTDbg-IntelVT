#pragma once

#include <string>
#include <Windows.h>

#include "Blowfish.h"

extern const unsigned int scm_auiInitP[MAX_PBLOCK_SIZE];
extern const unsigned int scm_auiInitS[MAX_SBLOCK_XSIZE][MAX_SBLOCK_YSIZE];

void EncryptBlock(Blowfish* blowfish, SBlock* block);
void DecryptBlock(Blowfish* blowfish, SBlock* block);
int EncryptByte(Blowfish* blowfish, const unsigned char* bin, size_t n, unsigned char* bout, int iMode);
int DecryptByte(Blowfish* blowfish, const unsigned char* bin, size_t n, unsigned char* bout, int iMode);
