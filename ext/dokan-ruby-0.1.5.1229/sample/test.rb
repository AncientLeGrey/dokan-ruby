
require 'dokan_lib'

class Hello

  def initialize
    @hello = "hello, world"
  end

  def open(path, fileinfo)
    puts "#open " + path
    path == "/hello.txt" or path == "/"
  end

  def create(path, fileinfo)
    puts "#create " + path
    false
  end

  def truncate(path, length, fileinfo)
    puts "#truncate " + path
    false
  end

  def opendir(path, fileinfo)
    puts "#opendir " + path
    true
  end

  def mkdir(path, fileinfo)
    puts "#mkdir " + path
    false
  end

  def close(path, fileinfo)
    puts "#close " + path
    true
  end

  def cleanup(path, fileinfo)
    puts "#cleanup " + path
    true
  end

  def read(path, offset, length, fileinfo)
    puts "#read " + path
    if offset < @hello.length
      @hello[offset, length]
    else
      false
    end
  end

  def write(path, offset, data, fileinfo)
    puts "#write " + path
    false
  end

  def flush(path, fileinfo)
    true
  end

  def stat(path, fileinfo)
    puts "#fstat " + path
    #[size, attr, ctime, atime, mtime]
    if path == "/hello.txt"
      [@hello.length, Dokan::NORMAL, 0, 0, 0]
    elsif path == "/"
      [0, Dokan::DIRECTORY, 0, 0, 0]
    else
      false
    end
  end

#  def readdira(path, fileinfo)
#  end

  def readdir(path, fileinfo)
    puts "#readdir " + path
    ["hello.txt"]
  end

  def setattr(path, attr, fileinfo)
    puts "#setattr " + path
    false
  end

  def utime(path, ctime, atime, mtime, fileinfo)
    puts "#utime " + path
    false
  end

  def remove(path, fileinfo)
    puts "#remove " + path
    false
  end

  def rename(path, newpath, fileinfo)
    false
  end

  def rmdir(path, fileinfo)
    false
  end

  def unmount(fileinfo)
    puts "#unmount"
    true
  end
end



Dokan.mount("r", Hello.new)

