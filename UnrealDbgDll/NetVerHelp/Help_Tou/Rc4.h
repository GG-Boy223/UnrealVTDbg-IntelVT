#pragma once
#ifndef _ENCRYPT_RC4_
#define _ENCRYPT_RC4_
#define BOX_LEN 256
namespace Help
{
	class Rc4
	{
	public:
		// 加密，返回加密结果
		char* Encrypt(const char* szSource, const char* szPassWord); 
		// 解密，返回解密结果
		char* Decrypt(const char* szSource, const char* szPassWord); 
	private:
		int GetKey(const unsigned char* pass, int pass_len, unsigned char *out);
		int RC4(const unsigned char* data, int data_len, const unsigned char* key, int key_len, unsigned char* out, int* out_len);
		static void swap_byte(unsigned char* a, unsigned char* b);
		char* ByteToHex(const unsigned char* vByte, const int vLen); // 把字节码pbBuffer转为十六进制字符串，方便传输
		unsigned char* HexToByte(const char* szHex); // 把十六进制字符串转为字节码pbBuffer，解码
	};
}
#endif // #ifndef _ENCRYPT_RC4_