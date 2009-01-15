class PokerBot
  def initialize
    # Shortcut for OpenHoldem
    @oh = OpenHoldem
  end
  
  def process_state(state)
    # The state is passed in a raw string containing the structure that must be decoded if needed
    0.0
  end

  def process_query(query)
    puts "Received query: #{query.inspect}"

    case query
    when "dll$rais" # OpenHoldem found dll$rais in a formula and requests a value
      # Get hole card ranks
      r0 = @oh["$$pr0"]
      r1 = @oh["$$pr1"]

      if r0 == r1 and r0 >= 10 # If it's a pair above TT
	return 1.0
      end
    end
    # Return 0 in other cases
    0.0
  end
end

# Create a new bot instance
bot = PokerBot.new

# Tells OpenHoldem module to process requests on this instance
OpenHoldem.bot = bot
