#ifndef LIBMEDIATION_FILE_H
#define LIBMEDIATION_FILE_H

#include <stdexcept>
#include <fcntl.h>
#include "../types/types.h"

#ifdef WIN32
#pragma warning( disable : 4290 )
#endif

class AbstractStream
{
public:
    AbstractStream() {}
    virtual ~AbstractStream() {}

    static const unsigned int ofRead = 1;
    static const unsigned int ofWrite = 2;
    static const unsigned int ofAppend = 4;			// file can be exist
    static const unsigned int ofOpenExisting = 8;	// do not create file if absent
    static const unsigned int ofCreateNew = 16;		// create new file. Return error If file exist 
    static const unsigned int ofNoTruncate = 32;	// keep file data while opening

    virtual bool open(const char* fName, unsigned int oflag, unsigned int systemDependentFlags = 0 ) = 0;
    virtual bool close() = 0;
    virtual int64_t size() const = 0;
};

class AbstractOutputStream: public AbstractStream
{
public:
    AbstractOutputStream(): AbstractStream() {}
    virtual int write( const void* buffer, uint32_t count ) = 0;
    virtual void sync() = 0;
};


//! Класс, предоставляющий интерфейс для работы с файлами
class  File: public AbstractOutputStream
{
public:
	enum SeekMethod
	{
		smBegin,
		smCurrent,
		smEnd
	};

	File();
	//! Конструктор
	/*!
		Создаёт объект и открывает файл. Если файл открыть не удалось, то кидается исключение.
		\param fName Имя файла
		\param oflag Битовая маска параметров открываемого файла
		\param systemDependentFlags Системно-зависимые флаги для открытия файла. 
			В реализации для win32 значение этого параметра передаётся в параметр dwFlagsAndAttributes функции CreateFile,
			в реализации для unix - во второй параметр(oflag) функции open.
	*/
	File( 
		const char* fName, 
		unsigned int oflag,
		unsigned int systemDependentFlags = 0 ) /* throw ( std::runtime_error ) */;
	virtual ~File();

	//! Открытие файла
	/*!
		Если файл уже открыт, то он закрывается
		\param fName Имя файла
		\param oflag Битовая маска параметров открываемого файла
		\param systemDependentFlags Системно-зависимые флаги для открытия файла. 
			В реализации для win32 значение этого параметра передаётся в параметр dwFlagsAndAttributes функции CreateFile,
			в реализации для unix - во второй параметр(oflag) функции open.
		\return true, если файл удачно открыт, false в противном случае
	*/
	virtual bool open( 
		const char* fName, 
		unsigned int oflag,
		unsigned int systemDependentFlags = 0 ) override;
	//! Закрыть файл
	/*!
		\return true, если файл закрыт, false в случае ошибки
	*/
	virtual bool close() override;

	//! Чтение файла
	/*!
		Читает в буфер максимум count байт из файла. Возвращает число считанных из файла байт.
		\return Число считанных из файла байт, 0 при попытке чтения из конца файла, -1 в случае ошибки чтения.
	*/
	int read( void* buffer, uint32_t count ) const;
	//! Запись в файл
	/*!
		\return Число байт, записанных в файл. -1 в случае ошибки (например, недостаточно места на диске).
	*/
	virtual int write( const void* buffer, uint32_t count ) override;
	//!Сбросить изменения на диск
	/*!
		Сброс изменений на диск
	*/
	virtual void sync() override;

	//! Проверка, открыт ли файл
	/*!
		\return true, если файл открыт, false в противном случае.
	*/
	bool isOpen() const;

	//!Получение размера файла
	/*!
		\return Текущий размер файла
	*/
	bool size( uint64_t* const fileSize ) const;

    virtual int64_t size() const override {
        uint64_t result;
        if (size(&result))
            return (int64_t) result;
        else
            return -1;
    }

	//!Перемещение курсора по файлу
	/*!
		\param offset 
		\param whence
		\return положение указателя после перемещения, или uint64_t(-1) в случае ошибки.
	*/
	uint64_t seek( int64_t offset, SeekMethod whence = smBegin);

	//!Изменение размера файла
	/*!
		Положение файлового курсора после вызова этой функции не определено.
		\param newFileSize Новый размер файла. Эта функция может как увеличить размер файла, так и сократить его.
	*/
	bool truncate( uint64_t newFileSize);
	
	std::string getName() { return m_name; }

    uint64_t pos() const { return m_pos; }

private:
	void* m_impl;

	friend class MemoryMappedFile;
	
	std::string m_name;
    mutable uint64_t m_pos;
};

class FileFactory
{
public:
    virtual ~FileFactory() {}
    virtual AbstractOutputStream* createFile() = 0;
    virtual bool isVirtualFS() const = 0;
};

#endif	//LIBMEDIATION_FILE_H
