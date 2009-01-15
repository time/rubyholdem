class PokerBot
  def initialize
    @oh = OpenHoldem
  end
  
  def process_state(state)
    0.0
  end

  def process_query(query)
    puts "Received query: #{query.inspect}"
    if query == "dll$alli"
      1.0
    else
      0.0
    end
  end
end

OpenHoldem.bot = PokerBot.new
