
module OpenHoldem
  class_eval """
    class <<self
      attr_accessor :bot, :pokerbot_mtime
    end
  """
  
  module_function

  def process_query(name)
    reload_pokerbot
    @bot.process_query(name)
  end
  
  def process_state(state)
    reload_pokerbot
    @bot.process_state(state)
  end
  
  def process_event name
    if name == "load"
      load_pokerbot
    end
    0.0
  end
  
  def pokerbot_filename
    File.join(Dir.pwd, "pokerbot.rb")
  end
  
  def load_pokerbot
    file = pokerbot_filename
    @pokerbot_mtime = File.mtime(file)
    Kernel.load(file)
  end
  
  def reload_pokerbot
    file = pokerbot_filename
    load_pokerbot if File.mtime(file) != @pokerbot_mtime
  end
  
  def [](symbol)
    if symbol =~ /^dll\$/
      raise "dll$ symbols blocked due to threading issues (requested #{symbol.inspect})"
    end
    chair = get_symbol(0, "userchair")
    get_symbol(chair, symbol.to_s)
  end
  
end
