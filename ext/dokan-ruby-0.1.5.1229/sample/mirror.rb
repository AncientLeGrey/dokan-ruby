
require 'dokan_lib'

class Mirror

  def initialize(dir)
    @dir = dir
  end

  def get_path(path)
    @dir + path.sub("/", "\\")
  end

  def open(path, fileinfo)
    puts "#open " + path
    if path == "/"
      true
    else
      File.file?(get_path(path))
    end
  end

  def create(path, fileinfo)
    puts "#create " + path
    File.open(get_path(path), File::CREAT|File::BINARY|File::WRONLY) { true }
  rescue
    false
  end

  def truncate(path, length, fileinfo)
    puts "#truncate " + path
    File.truncate(get_path(path), length)
    true
  rescue
    false
  end

  def opendir(path, fileinfo)
    puts "#opendir " + path
   if path == "/"
      true
   else
      File.directory?(get_path(path))
   end
  end

  def mkdir(path, fileinfo)
    puts "#mkdir " + path
    Dir.mkdir(get_path(path))
    true
  rescue
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
    puts "#read " + path + " length " + length.to_s + " offset " + offset.to_s;
    if File.file?(get_path(path))
      File.open(get_path(path), "rb") do |file|
        file.seek(offset)
        str = file.read(length)
        str = "" if str == nil
        str
      end
#      File.read(get_path(path), length, offset)
    else
      false
    end
  rescue
    false
  end

  def write(path, offset, data, fileinfo)
    puts "#write " + path
    if File.writable?(get_path(path))
      File.open(get_path(path), "r+") do |file|
        file.seek(offset)
        file.write(data)
      end
    else
      false
    end
  rescue
    false
  end
  
  def flush(path, fileinfo)
    true
  end

  def stat(path, fileinfo)
    puts "#stat " + path
    #[size, attr, ctime, atime, mtime]
    if File.exist?(get_path(path))
      s = File.stat(get_path(path)) 
      [s.size,
        s.directory? ? Dokan::DIRECTORY : Dokan::NORMAL,
        s.ctime.to_i, s.atime.to_i, s.mtime.to_i]
    else
      false
    end
  rescue
    false
  end

#  def readdira(path, fileinfo)
#  end

  def readdir(path, fileinfo)
    puts "#readdir " + path
    if File.directory?(get_path(path))
      Dir.entries(get_path(path))
    else
      false
    end

  rescue
    false
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
    File.delete(get_path(path))
  rescue
    false
  end

  def rename(path, newpath, fileinfo)
    File.rename(get_path(path), get_path(newpath))
    true
  rescue
    false
  end

  def rmdir(path, fileinfo)
    Dir.rmdir(get_path(path))
    true
  rescue
    false
  end

  def lock(path, offset, length, fileinfo)
    true
  end

  def unlock(path, offset, length, fileinfo)
    true
  end

  def unmount(fileinfo)
    puts "#unmount"
    true
  end
end



Dokan.mount("r", Mirror.new("e:\\"))

