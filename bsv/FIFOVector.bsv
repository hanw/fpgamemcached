import BRAM::*;
import Ehr::*;
import GetPut::*;
import ClientServer::*;
import FIFO::*;
import Vector::*;
import ControllerTypes::*;


interface FIFOVector#(numeric type n, type t, numeric type depth);
   method Action enq(Bit#(TLog#(n)) tag, t data);
   interface Server#(Bit#(TLog#(n)), t) deqServer;
endinterface

module mkBRAMFIFOVector(FIFOVector#(n, t, depth))
   provisos(Bits#(t, tSz),
            Add#(depth, 1, depth1),
            Log#(depth1, sz),
            Add#(sz, 1, sz1),
            Log#(n, nlog),
            Log#(depth, depthlog),
            Add#(a__, TLog#(n), TAdd#(nlog, depthlog)),
            Add#(b__, sz1, TAdd#(nlog, depthlog)));
   
   Integer depthi = valueOf(depth);
   Bit#(sz1) nb = fromInteger(depthi);
   Bit#(sz1) n2 = 2*nb;
   BRAM2Port#(Bit#(TAdd#(nlog, depthlog)), t) fifostore <- mkBRAM2Server(defaultValue);
   //Vector#(n, Reg#(t)) data <- replicateM(mkRegU);
   Vector#(n, Ehr#(2, Bit#(sz1))) enqP <- replicateM(mkMyEhr(0));
   Vector#(n, Ehr#(2, Bit#(sz1))) deqP <- replicateM(mkMyEhr(0));
   Vector#(n, Ehr#(2, Bool)) enqEn <- replicateM(mkMyEhr(True));
   Vector#(n, Ehr#(2, Bool)) deqEn <- replicateM(mkMyEhr(False));
   Ehr#(2, t) tempData <- mkMyEhr(?);
   Ehr#(2, Maybe#(Tuple2#(Bit#(TLog#(n)), Bit#(sz1)))) tempEnqP <- mkMyEhr(Invalid);

   //for (Integer i = 0; i < valueOf(n); i = i + 1) begin
   rule canonicalize;
      for ( Integer i = 0; i < valueOf(n); i = i + 1) begin
         Bit#(sz1) cnt = enqP[i][1] >= deqP[i][1]? enqP[i][1] - deqP[i][1]: (enqP[i][1]%nb + nb) - deqP[i][1]%nb;
         if(!enqEn[i][1] && cnt != nb) enqEn[i][1] <= True;
         if(!deqEn[i][1] && cnt != 0) deqEn[i][1] <= True;
      end
      
      if(isValid(tempEnqP[1]))
         begin
            let v = validValue(tempEnqP[1]);
            let tag = tpl_1(v);
            let ptr = tpl_2(v);
            fifostore.portA.request.put(BRAMRequest{write:True,
                                                    responseOnWrite: False,
                                                    address: (extend(tag) << valueOf(nlog)) + extend(ptr),
                                                    datain: tempData[1]});
            
            //data[i][validValue(tempEnqP[i][1])] <= tempData[i][1];
            $display("%m, canonicalize tag = %d, ptr = %d, data = %h", tag, ptr, tempData[1]);
            tempEnqP[1] <= Invalid;
         end
   endrule
   //end
   
   
   FIFO#(Bit#(TLog#(n))) deqReqQ <- mkFIFO();
   FIFO#(Tuple2#(Bit#(TLog#(n)), t)) enqReqQ <- mkFIFO();
   rule doEnq if (enqEn[tpl_1(enqReqQ.first)][0]);

      let v <- toGet(enqReqQ).get();
      let tag = tpl_1(v);
      let data = tpl_2(v);
      tempData[0] <= data;
      $display("%m, doEnq tag = %d, data = %h", tag, data);
      tempEnqP[0] <= Valid (tuple2(tag, enqP[tag][0]%nb));
      enqP[tag][0] <= (enqP[tag][0] + 1)%n2;
      enqEn[tag][0] <= False;
   endrule
      
   
   rule doDeq if (deqEn[deqReqQ.first][0]);
      let tag <- toGet(deqReqQ).get;
      $display("%m, doDeq tag = %d", tag);

      fifostore.portB.request.put(BRAMRequest{write:False,
                                              responseOnWrite: False,
                                              address: (extend(tag) << valueOf(nlog)) + extend(deqP[tag][0]%nb),
                                              datain: ?});
      deqP[tag][0] <= (deqP[tag][0] + 1)%n2;
      deqEn[tag][0] <= False;
   endrule

   
   
   method Action enq(Bit#(TLog#(n)) tag, t data);
      $display("%m Got Request, tag = %d, data = %h", tag, data);
      enqReqQ.enq(tuple2(tag,data));
   endmethod
   
   interface Server deqServer;
      interface Put request=toPut(deqReqQ);
      interface Get response=fifostore.portB.response;
   endinterface
   
endmodule
   
