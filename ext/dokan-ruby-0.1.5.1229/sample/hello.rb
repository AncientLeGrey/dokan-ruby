require 'dokanfs'

class Hello

  def initialize
   @msg = "hello, world"
  end

  def contents path
    puts "##contents " + path
    ["hello.txt"]
  end

  def file? path
    puts "##file? " + path
    path  =~ /hello.txt/
  end

  def directory? path
    puts "##directory? " + path
    path == "/"
  end

  def read_file path
    puts "##read_file " + path
    @msg
  end

  def size path
    puts "##size " + path
    @msg.length
  end
end


FuseFS.set_root(Hello.new)
FuseFS.mount_under("r")
FuseFS.run

