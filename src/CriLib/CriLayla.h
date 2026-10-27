#pragma once

namespace CriLib {
	class CriLayla {
	protected:
		const unsigned char *buffer;
		const unsigned char *address;
		unsigned int bits;
		unsigned int remaining;

		unsigned int readBits(unsigned int count);

		void writeBits(vector<unsigned char> &data, unsigned int value, unsigned int count);

	public:
		static bool decompress(
			const unsigned char *buffer,
			size_t size,
			unsigned char *data,
			size_t extract_size
		);

		static bool compress(
			const unsigned char *data,
			size_t size,
			vector<unsigned char> &buffer
		);
	};
};
