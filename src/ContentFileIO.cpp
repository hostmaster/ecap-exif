#include "ContentFileIO.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>

#include <libecap/common/errors.h>

#include "Log.hpp"
#include "MemoryAreaDetails.hpp"

using namespace ExifAdapter;

// FIXME: should be configurable
const char* TEMPORARY_FILENAME_FORMAT = "/tmp/exif-ecap-XXXXXX";

//------------------------------------------------------------------------------
libecap::shared_ptr<ContentFileIO> ContentFileIO::FromTemporaryFile()
{
    // FIXME: correctly determine length of format
    const int filename_length = 32;
    char temporary_filename[filename_length];
    memset(temporary_filename, 0, filename_length);
    strncpy(temporary_filename, TEMPORARY_FILENAME_FORMAT, filename_length);

    int result = mkstemp(temporary_filename);
    if (result == -1)
    {
         Log(libecap::flXaction | libecap::ilDebug)
             << "failed to create temporary file with format "
             << temporary_filename
             << ": " << strerror(errno);
        return libecap::shared_ptr<ContentFileIO>();
    }

    return libecap::shared_ptr<ContentFileIO>(
        new ContentFileIO(result, temporary_filename));
}

//------------------------------------------------------------------------------
ContentFileIO::ContentFileIO(
    int fd,
    const std::string& filename)
    : fd(fd)
    , filename(filename)
{
}

//------------------------------------------------------------------------------
ContentFileIO::~ContentFileIO()
{
    CloseFile();
    RemoveFile();
}

//------------------------------------------------------------------------------
size_t ContentFileIO::Write(const libecap::Area& data)
{
    if (!fd && !OpenFile())
    {
        throw std::runtime_error("Failed to open file");
    }

    const size_t written = write(fd, data.start, data.size);
    if (written != data.size)
    {
        throw std::runtime_error(strerror(errno));
    }

    return written;
}

//------------------------------------------------------------------------------
void ContentFileIO::Shift(libecap::size_type bytes)
{
    offset += bytes;
}

//------------------------------------------------------------------------------
void ContentFileIO::ResetOffset()
{
    offset = 0;
}

//------------------------------------------------------------------------------
libecap::Area ContentFileIO::Read(
    libecap::size_type offset,
    libecap::size_type size)
{
    if (size == 0)
    {
        return libecap::Area();
    }

    Must(fd);

    const libecap::size_type position = this->offset + offset;
    Must(lseek(fd, position, SEEK_SET) != -1);

    libecap::shared_ptr<MemoryAreaDetails> details(
        new MemoryAreaDetails(size));

    const ssize_t result = read(fd, details->GetAreaStart(), size);
    if (result != -1)
    {
        return libecap::Area(details->GetAreaStart(), result, details);
    }

    return libecap::Area();
}

//------------------------------------------------------------------------------
void ContentFileIO::ApplyFilter(libecap::shared_ptr<MetadataFilter> filter)
{
    CloseFile();

    filter->ProcessFile(filename);

    if (!OpenFile())
    {
        throw std::runtime_error("Failed to open processed file");
    }
}

//------------------------------------------------------------------------------
uint64_t ContentFileIO::GetLength() const
{
    struct stat statbuf;

    if (fstat(fd, &statbuf) == -1)
    {
        throw std::runtime_error(strerror(errno));
    }

    return statbuf.st_size;
}

//------------------------------------------------------------------------------
bool ContentFileIO::OpenFile()
{
    fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        fd = 0;
        Log(libecap::flXaction | libecap::ilDebug)
            << "failed to open file "
            << filename
            << ": " << strerror(errno);
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
void ContentFileIO::CloseFile()
{
    if (fd)
    {
        if (close(fd) != 0)
        {
            Log(libecap::flXaction | libecap::ilDebug)
                << "failed to close file "
                << filename << ": "
                << strerror(errno);
        }

        fd = 0;
    }

}

//------------------------------------------------------------------------------
void ContentFileIO::RemoveFile()
{
    if (!filename.empty())
    {
        if (remove(filename.c_str()) != 0)
        {
            Log(libecap::flXaction | libecap::ilDebug)
                << "failed to remove file "
                << filename << ": "
                << strerror(errno);
        }

        filename.clear();
    }

}
