
require 'dokan_lib'

class DokanProxy


  def initialize
    @root = nil
  end

  def set_root(root)
    @root = root
    def @root.method_missing(name, *arg)
      puts "#method_missing " + name.to_s
      false
    end
  end

  def open(path, fileinfo)
    puts "#open " + path
    if path == "/"
      true
    else
      @root.file?(path) or @root.directory?(path)
    end
  end

  def create(path, fileinfo)
    puts "#create " + path
    if @root.can_write?(path)
      @root.write_to(path, "")
      true
    else
      false
    end
  end

  def truncate(path, fileinfo)
    puts "#truncate " + path
    @root.can_write?(path) and @root.write_to(path, "")
  end

  def opendir(path, fileinfo)
    puts "#opendir " + path
    if path == "/"
      true
    else
      @root.directory?(path)
    end
  end

  def mkdir(path, fileinfo)
    puts "#mkdir " + path
    @root.can_mkdir?(path) and @root.mkdir(path)
  end

  def cleanup(path, fileinfo)
    puts "#cleanup " + path
    if fileinfo.context
      @root.can_write?(path) and @root.write_to(path, fileinfo.context)
    end
    true
  end

  def close(path, fileinfo)
    puts "#close " + path
    true
  end

  def read(path, offset, length, fileinfo)
    puts "#read " + path
    return "" unless @root.file?(path)
    str = @root.read_file(path)
    if offset < str.length
      str[offset, length]
    else
      false
    end
  end

  def write(path, offset, data, fileinfo)
    puts "#write " + path
    fileinfo.context = "" unless fileinfo.context
    fileinfo.context[offset, data.length] = data
    true
  end

  def flush(path, fileinfo)
    puts "#flush " + path
    true
  end

  def stat(path, fileinfo)
    puts "#stat " + path
    #[size, attr, ctime, atime, mtime]

    size = @root.size(path)
    size = 0 unless size

    if path == "/" or @root.directory?(path)
        [size, Dokan::DIRECTORY, 0, 0, 0]
    else
        [size, Dokan::NORMAL, 0, 0, 0]
    end
  end

#  def readdira(path, fileinfo)
#  end

  def readdir(path, fileinfo)
    puts "#readdir " + path
    path == "/" or @root.directory?(path) and @root.contents(path)
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
    @root.can_delete?(path) and @root.delete(path)
  end

  def rename(path, newpath, fileinfo)
    puts "#rename " + path
    if @root.file?(path) and @root.can_delete?(path) and @root.can_write?(path)
      str = @root.read_file(path)
      @root.write_to(newpath, str)
      @root.delete(path)
    else
     false
    end
  end

  def rmdir(path, fileinfo)
    puts "#rmdir " + path
    @root.can_rmdir(path) and @root.rmdir(path)
  end

  def lock(path, fileinfo)
    true
  end

  def unlock(path, fileinfo)
    true
  end

  def unmount(fileinfo)
    puts "#unmount"
    @root.unmount
  end


end


#
# from fusefs.rb
# Author: Greg Millam <walker@deafcode.com>.
#
module DokanFS


  def DokanFS.set_root(root)
    @proxy = DokanProxy.new
    @proxy.set_root(root)
  end

  def DokanFS.mount_under(letter)
    @letter = letter
  end

  def DokanFS.run
    Dokan.mount(@letter, @proxy)
  end


  class DokanDir
    def split_path(path)
      cur, *rest = path.scan(/[^\/]+/)
      if rest.empty?
        [ cur, nil ]
      else
        [ cur, File.join(rest) ]
      end
    end
    def scan_path(path)
      path.scan(/[^\/]+/)
    end
  end

  FuseDir = DokanDir

  class MetaDir < DokanDir
    def initialize
      @subdirs  = Hash.new(nil)
      @files    = Hash.new(nil)
    end

    # Contents of directory.
    def contents(path)
      base, rest = split_path(path)
      case
      when base.nil?
        (@files.keys + @subdirs.keys).sort.uniq
      when ! @subdirs.has_key?(base)
        nil
      when rest.nil?
        @subdirs[base].contents('/')
      else
        @subdirs[base].contents(rest)
      end
    end

    # File types
    def directory?(path)
      base, rest = split_path(path)
      case
      when base.nil?
        true
      when ! @subdirs.has_key?(base)
        false
      when rest.nil?
        true
      else
        @subdirs[base].directory?(rest)
      end
    end
    def file?(path)
      base, rest = split_path(path)
      case
      when base.nil?
        false
      when rest.nil?
        @files.has_key?(base)
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].file?(rest)
      end
    end

    # File Reading
    def read_file(path)
      base, rest = split_path(path)
      case
      when base.nil?
        nil
      when rest.nil?
        @files[base].to_s
      when ! @subdirs.has_key?(base)
        nil
      else
        @subdirs[base].read_file(rest)
      end
    end

    # Write to a file
    def can_write?(path)
      #return false unless Process.uid == FuseFS.reader_uid
      base, rest = split_path(path)
      case
      when base.nil?
        true
      when rest.nil?
        true
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].can_write?(rest)
      end
    end
    def write_to(path,file)
      base, rest = split_path(path)
      case
      when base.nil?
        false
      when rest.nil?
        @files[base] = file
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].write_to(rest,file)
      end
    end

    # Delete a file
    def can_delete?(path)
      #return false unless Process.uid == FuseFS.reader_uid
      base, rest = split_path(path)
      case
      when base.nil?
        false
      when rest.nil?
        @files.has_key?(base)
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].can_delete?(rest)
      end
    end
    def delete(path)
      base, rest = split_path(path)
      case
      when base.nil?
        nil
      when rest.nil?
        # Delete it.
        @files.delete(base)
      when ! @subdirs.has_key?(base)
        nil
      else
        @subdirs[base].delete(rest)
      end
    end

    # Make a new directory
    def can_mkdir?(path)
      #return false unless Process.uid == FuseFS.reader_uid
      base, rest = split_path(path)
      case
      when base.nil?
        false
      when rest.nil?
        ! (@subdirs.has_key?(base) || @files.has_key?(base))
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].can_mkdir?(rest)
      end
    end
    def mkdir(path,dir=nil)
      base, rest = split_path(path)
      case
      when base.nil?
        false
      when rest.nil?
        dir ||= MetaDir.new
        @subdirs[base] = dir
        true
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].mkdir(rest,dir)
      end
    end

    # Delete an existing directory.
    def can_rmdir?(path)
      #return false unless Process.uid == FuseFS.reader_uid
      base, rest = split_path(path)
      case
      when base.nil?
        false
      when rest.nil?
        @subdirs.has_key?(base)
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].can_rmdir?(rest)
      end
    end
    def rmdir(path)
      base, rest = split_path(path)
      dir ||= MetaDir.new
      case
      when base.nil?
        false
      when rest.nil?
        @subdirs.delete(base)
        true
      when ! @subdirs.has_key?(base)
        false
      else
        @subdirs[base].rmdir(rest,dir)
      end
    end
  end
end


FuseFS = DokanFS


