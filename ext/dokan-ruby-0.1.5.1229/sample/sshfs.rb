require 'dokan_lib'
require 'net/sftp'

#GC.disable

class Sshfs

  def initialize(sftp)
    @sftp = sftp
  end

  def open(path, fileinfo)
    puts "#open " + path
    @sftp.open_handle(path, "r") do
      true
    end
  rescue
    p $!
    false
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
    if path == "/"
      true
    elsif directory?(path)
      true
    else
      false
    end
  rescue
    p $!
    false
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
    data = nil
    @sftp.open_handle(path, "r") do |handle|
      data = @sftp.read(handle)
    end
    data != nil ? data[offset, length] : false
  rescue
    p $!
    false
  end

  def write(path, offset, data, fileinfo)
    puts "#write " + path
    false
  end

  def flush(path, fileinfo)
    true
  end

  def directory?(path)
    0 < (@sftp.stat(path).permissions & 040000)
  end

  def stat(path, fileinfo)
    puts "#stat " + path
    #[size, attr, ctime, atime, mtime]
    if path == "/"
      [0, Dokan::DIRECTORY, 0, 0, 0]
    else
      s = @sftp.stat(path)
      [s.size,
        0 < (s.permissions & 040000) ? Dokan::DIRECTORY : Dokan::NORMAL,
        s.mtime.to_i, s.atime.to_i, s.mtime.to_i]
     end
  rescue
    p $!
    false
  end

#  def readdira(path, fileinfo)
#  end

  def readdir(path, fileinfo)
    puts "#readdir " + path
    entries = []
    handle = @sftp.opendir(path)
    items = @sftp.readdir(handle)
    items.each do |item|
      entries.push item.filename
    end
    @sftp.close_handle(handle)
    entries
  rescue
    p $!
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


#if (ARGV.size != 4)
#  puts "Usage: #{$0} letter host usr password"
#  exit
#end


@host = "****"
@user = "****"
@passwd = "****"
Net::SFTP.start(@host, @user, @passwd) do |ssh|
  Dokan.mount("s", Sshfs.new(ssh))
end


