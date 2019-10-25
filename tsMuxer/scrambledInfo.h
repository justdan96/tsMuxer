#ifndef __SCRAMBLED_INFO_H
#define __SCRAMBLED_INFO_H

#include <map>
#include <vector>
#include <types/types.h>

#include <fs/systemlog.h>

struct ScrambledData {
	uint32_t m_start;
	uint32_t m_len;
	int m_code;
	ScrambledData(uint32_t start, uint32_t len, int code): m_start(start), m_len(len), m_code(code) {}
};

class ScrambledInfo {
public:
	ScrambledInfo() { m_scrambledIndex = m_offset = 0; }
	void clear() { 
		m_data.clear(); 
		m_scrambledIndex = 0;
		m_offset = 0;
	}
	const size_t size() { return m_data.size(); }
	void addValue(uint32_t start, uint32_t len, int code) 
	{
		if (m_data.size() > 0 && m_data[m_data.size()-1].m_start == start && m_data[m_data.size()-1].m_code == code)
			m_data[m_data.size()-1].m_len += len;
		else
			m_data.push_back(ScrambledData(start, len, code));
	};
	
	int getScrambledIndex(uint32_t offset) {
		for (; m_scrambledIndex < m_data.size(); ++m_scrambledIndex)
			if (m_data[m_scrambledIndex].m_start - m_offset >= offset) 
				return (int) m_scrambledIndex;
		return -1;
	}

	bool isScrambled(uint32_t offset) {
		//LTRACE(6, 0, "ggg = " << offset);
		for (size_t i = m_scrambledIndex; i < m_data.size(); ++i) {
			size_t start = m_data[i].m_start - m_offset;
			size_t len = m_data[i].m_len;
			//LTRACE(6, 0, "start = " << start << "end=" << start+len);
			if (start > offset)
				return false;
			else if (start <= offset && start + len >= offset) 
				return true;
		}
		//LTRACE(6, 0, "end of ggg");
		return false;
	}

	void setDataOffset(size_t offset) {m_offset = offset;}


	//const getData() {return m_data;}
	//const ScrambledData& operator[](size_t index) { return m_data[index]; }
	//const ScrambledData& getData(size_t index) {return m_data[index];}
	const uint32_t getDataStart(size_t index) {return m_data[index].m_start - m_offset;}
	const uint32_t getDataLen(size_t index) {return m_data[index].m_len;}
	const uint32_t getDataCode(size_t index) {return m_data[index].m_code;}

private:
	size_t m_scrambledIndex;
	std::vector<ScrambledData> m_data;
	size_t m_offset;
};

#endif
