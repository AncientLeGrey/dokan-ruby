require 'rss'
require 'dokanfs'

class RSSFS
  def initialize table
    @files = Hash.new
    @files["/"] = Array.new
    table.each do |k,v|
      rss =  RSS::Parser.parse(v)
      rss.output_encoding = "Shift_JIS"
      @files["/"].push k
      @files["/#{k}"] = Array.new
      rss.items.each do |item|
        @files["/#{k}"].push "#{item.title}.txt"
        @files["/#{k}/#{item.title}.txt"] = item.description
      end
    end
  end

  def file? path
    @files[path].instance_of? String
  end
  
  def directory? path
    @files[path.sub(/(.+)\/$/, '\1')].instance_of? Array
  end

  def contents path
    if @files[path.sub(/(.+)\/$/, '\1')].instance_of? Array
      @files[path.sub(/(.+)\/$/, '\1')]
    else
      false
    end
  end

  def read_file path
    @files[path]
  end

  def size path
    if @files[path].instance_of? String
      @files[path].size
    else
      0
    end
  end
end

rssfs = RSSFS.new( {"decas"=> "http://decas-dev.net/feed/", "hatena_hotentry" => "http://b.hatena.ne.jp/hotentry?mode=rss"} )
DokanFS.set_root(rssfs)
DokanFS.mount_under("r")
DokanFS.run
