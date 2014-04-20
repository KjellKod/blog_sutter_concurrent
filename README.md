concurrent<...object...> wrapper
=================================

Makes access to *any* object asynchronous. All asynchronous calls will be executed in the background and in FIFO order.
Many actions can be bundled together in one asynchronous operation. All calls are made through a lambda call that has to take a reference to the wrapped object as input argument. 


The code is modified from Herb Sutters example of Concurrent object wrapper.   
http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Herb-Sutter-Concurrency-and-Parallelism


It is slightly modified to allow in-place object creation instead of copy of object. 
See blog post at: http://kjellkod.wordpress.comg/2014/04/07/concurrency-concurrent-wrapper/

Silly Example from the unit tests:

```cpp
   concurrent<Greeting> greeting{"Hello World"};

   // execute two Hello calls in one asynchronous operation. 
   std::future<std::string> response = greeting( 
         [](Greeting& g) { 
            std::string reply{g.Hello(123) + " " + g.Hello(456)}; 
            return reply;
         }
       ); 
       
   EXPECT_EQ(response.get(), "Hello World 123 Hello World 456");
```


If you want to see how this can be improved even further. Take a look at
https://github.com/KjellKod/Concurrent
