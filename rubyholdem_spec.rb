
require 'rubyholdem'

describe OpenHoldem do
  before do
    @o = OpenHoldem
    @pwd = "/foo/bar"
    Dir.stub!(:pwd).and_return { @pwd }
    @loads = 0
    Kernel.stub!(:load).with("#@pwd/pokerbot.rb").and_return { @loads += 1; true }
    File.stub!(:mtime).with("#@pwd/pokerbot.rb").and_return { @mtime }
    @mtime = Time.now
  end
  
  it "should load pokerbot.rb on load_pokerbot" do
    @o.load_pokerbot
    @loads.should == 1
  end

  it "should call load_pokerbot on event load and return 0.0" do
    @o.should_receive(:load_pokerbot).with().once
    @o.process_event("load").should == 0.0
  end

  it "should not load pokerbot.rb on event other than load and return 0.0" do
    @o.should_not_receive(:load_pokerbot)
    @o.process_event("foo").should == 0.0
  end
  
  it "should call get_symbol on []" do
    symbol = mock("symbol")
    symbol_to_s = mock("symbol#to_s")
    result = mock("result")
    chair = mock("chair")
    
    symbol.should_receive(:to_s).and_return(symbol_to_s)
    
    @o.should_receive(:get_symbol).once.with(0, "userchair").and_return(chair)
    @o.should_receive(:get_symbol).once.with(chair, symbol_to_s).and_return(result)
    
    @o[symbol].should == result
  end
  
  it "should reload_pokerbot and propagate queries to the bot when it process_query" do
    bot = mock("bot")
    query = mock("query")
    result = mock("result")
    
    @o.bot = bot
    
    pokerbot_reloaded = false
    @o.should_receive(:reload_pokerbot).with().once.and_return do
      pokerbot_reloaded = true
    end
    bot.should_receive(:process_query).with(query).and_return do
      pokerbot_reloaded.should == true
      result
    end
    @o.process_query(query).should == result
  end
  
  %w( process_state process_query ).each do |process_method|
    it "should reload_pokerbot and propagate to the bot when it #{process_method}" do
      bot = mock("bot")
      param = mock("param")
      result = mock("result")
      
      @o.bot = bot
      
      pokerbot_reloaded = false
      @o.should_receive(:reload_pokerbot).with().once.and_return do
        pokerbot_reloaded = true
      end
      bot.should_receive(process_method).with(param).and_return do
        pokerbot_reloaded.should == true
        result
      end
      @o.send(process_method, param).should == result
    end
  end
  
  it "should store mtime on load_pokerbot" do
    @o.pokerbot_mtime = "wrong mtime"
    @o.load_pokerbot
    @o.pokerbot_mtime.should == @mtime
  end
  
  # because when load fails we don't want to load it until it has changed again
  it "should store mtime before load on load_pokerbot" do
    @o.pokerbot_mtime = "wrong mtime"
    Kernel.should_receive(:load).and_return do
      @o.pokerbot_mtime.should == @mtime
    end
    @o.load_pokerbot
  end
  
  it "should load_pokerbot when mtime has changed on reload_pokerbot" do
    @o.load_pokerbot
    @mtime += 1
    @o.should_receive(:load_pokerbot).with().once
    @o.reload_pokerbot
  end
  
  it "should not load_pokerbot when mtime has not changed on reload_pokerbot" do
    @o.load_pokerbot
    @o.should_not_receive(:load_pokerbot)
    @o.reload_pokerbot
  end
  
  it "should refuse to get symbol from dll (because of thread issues)" do
    @o.should_not_receive(:get_symbol)
    lambda {
      @o["dll$foo"]
    }.should raise_error(
      RuntimeError,
      "dll$ symbols blocked due to threading issues (requested \"dll$foo\")"
    )
  end
end
