#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

void hexdump(const void* data, unsigned int dataLen);
