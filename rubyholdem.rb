=begin
  Copyright 2009 Time

  This file is part of RubyHoldem.

  RubyHoldem is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  RubyHoldem is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with RubyHoldem.  If not, see <http://www.gnu.org/licenses/>.
=end

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
