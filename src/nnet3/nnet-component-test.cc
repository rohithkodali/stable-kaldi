// nnet3/nnet-component-test.cc

// Copyright 2015  Johns Hopkins University (author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include "nnet3/nnet-nnet.h"
#include "nnet3/nnet-simple-component.h"
#include "nnet3/nnet-test-utils.h"

namespace kaldi {
namespace nnet3 {

// returns true if two are string are equal except for what looks like it might
// be a difference last digit of a floating point number, e.g. accept
// 1.234 to be the same as 1.235.  Not very rigorous.
static bool StringsApproxEqual(const std::string &a,
                               const std::string &b) {
  if (a == b || a.size() != b.size())
    return true;
  size_t size = a.size();
  for (size_t pos = 0; pos < size; pos++) {
    if (a[pos] != b[pos]) {
      if (!isdigit(a[pos]) || !isdigit(b[pos]))
        goto fail;
      // if it's not the last digit in the string, goto fail
      if (pos + 1 != size && isdigit(a[pos+1]))
        goto fail;
      size_t pos2;
      for (pos2 = pos - 1; pos2 > 0; pos2--) {
        if (a[pos2] == '.') break;  // we accept this difference: we went backwards and found a '.'
        if (!isdigit(a[pos2]))  // we reject this difference: we went back and
                                // found non-digit before '.' -> not floating
                                // point.
          goto fail;
      }
      if (pos2 == 0)
        goto fail;
    }
  }
  return true;
fail:
  KALDI_WARN << "Info strings differ: '" << a << "' vs. '" << b << "'.";
  return false;
}


void TestNnetComponentIo(Component *c) {
  bool binary = (Rand() % 2 == 0);
  std::ostringstream os1;
  c->Write(os1, binary);
  std::istringstream is(os1.str());
  Component *c2 = Component::ReadNew(is, binary);
  std::ostringstream os2;
  c2->Write(os2, binary);
  if (!binary) {
    KALDI_ASSERT(os2.str() == os1.str());
  }
  delete c2;
}

void TestNnetComponentCopy(Component *c) {
  Component *c2 = c->Copy();
  if (!StringsApproxEqual(c->Info(), c2->Info())) {
    KALDI_ERR << "Expected info strings to be equal: '"
              << c->Info() << "' vs. '" << c2->Info() << "'";
  }
  delete c2;
}

void TestNnetComponentAddScale(Component *c) {
  Component *c2 = c->Copy();
  Component *c3 = c2->Copy();
  c3->Add(0.5, *c2);
  c2->Scale(1.5);
  KALDI_ASSERT(StringsApproxEqual(c2->Info(), c3->Info()));
  delete c2;
  delete c3;
}

void TestNnetComponentVectorizeUnVectorize(Component *c) {
  if (!(c->Properties() & kUpdatableComponent))
    return;
  UpdatableComponent *uc = dynamic_cast<UpdatableComponent*>(c);
  KALDI_ASSERT(uc != NULL);
  UpdatableComponent *uc2 = dynamic_cast<UpdatableComponent*>(uc->Copy());
  uc2->SetZero(false);
  Vector<BaseFloat> params(uc2->NumParameters());
  uc2->Vectorize(&params);
  KALDI_ASSERT(params.Min()==0.0 && params.Sum()==0.0);
  uc->Vectorize(&params);
  uc2->UnVectorize(params);
  KALDI_ASSERT(StringsApproxEqual(uc2->Info(), uc->Info()));
  BaseFloat x = uc2->DotProduct(*uc2), y = uc->DotProduct(*uc),
      z = uc2->DotProduct(*uc);
  KALDI_ASSERT(ApproxEqual(x, y) && ApproxEqual(y, z));
  Vector<BaseFloat> params2(uc2->NumParameters());
  uc2->Vectorize(&params2);
  for(int i = 0; i < params.Dim(); i++)
    KALDI_ASSERT(params(i) == params2(i));
  delete uc2;
}

void TestStringsApproxEqual() {
  // we must test the test.
  KALDI_ASSERT(!StringsApproxEqual("a", "b"));
  KALDI_ASSERT(!StringsApproxEqual("1", "2"));
  KALDI_ASSERT(StringsApproxEqual("1.234", "1.235"));
  KALDI_ASSERT(StringsApproxEqual("x 1.234 y", "x 1.235 y"));
  KALDI_ASSERT(StringsApproxEqual("x 1.234 y 6.41", "x 1.235 y 6.49"));
}

void TestNnetComponentUpdatable(Component *c) {
  if (!(c->Properties() & kUpdatableComponent))
    return;
  UpdatableComponent *uc = dynamic_cast<UpdatableComponent*>(c);
  if (uc == NULL) {
    KALDI_ASSERT(!(c->Properties() & kUpdatableComponent) &&
                 "Component returns updatable flag but does not inherit "
                 "from UpdatableComponent");
    return;
  }
  if(!(uc->Properties() & kUpdatableComponent)){
    // testing that if it declares itself as non-updatable,
    // Scale() and Add() and SetZero() have no effect.
    KALDI_ASSERT(uc->NumParameters() == 0);
    KALDI_ASSERT(uc->DotProduct(*uc) == 0);
    UpdatableComponent *uc2 = dynamic_cast<UpdatableComponent*>(uc->Copy());
    uc2->Scale(7.0);
    uc2->Add(3.0, *uc);
    KALDI_ASSERT(StringsApproxEqual(uc2->Info(), uc->Info()));
    uc->SetZero(false);
    KALDI_ASSERT(StringsApproxEqual(uc2->Info(), uc->Info()));
    delete uc2;
  } else {
    KALDI_ASSERT(uc->NumParameters() != 0);
    UpdatableComponent *uc2 = dynamic_cast<UpdatableComponent*>(uc->Copy()),
        *uc3 = dynamic_cast<UpdatableComponent*>(uc->Copy());

    // testing some expected invariances of scale and add.
    uc2->Scale(5.0);
    uc2->Add(3.0, *uc3);
    uc3->Scale(8.0);
    // now they should both be scaled to 8 times the original component.
    if (!StringsApproxEqual(uc2->Info(), uc3->Info())) {
      KALDI_ERR << "Expected info strings to be equal: '"
                << uc2->Info() << "' vs. '" << uc3->Info() << "'";
    }
    // testing that scaling by 0.5 works the same whether
    // done on the vectorized paramters or via Scale().
    Vector<BaseFloat> vec2(uc->NumParameters());
    uc2->Vectorize(&vec2);
    vec2.Scale(0.5);
    uc2->UnVectorize(vec2);
    uc3->Scale(0.5);
    KALDI_ASSERT(uc2->Info() == uc3->Info());

    // testing that SetZero() works the same whether done on the vectorized
    // paramters or via SetZero(), and that unvectorizing something that's been
    // zeroed gives us zero parameters.
    uc2->Vectorize(&vec2);
    vec2.SetZero();
    uc2->UnVectorize(vec2);
    uc3->SetZero(false);
    uc3->Vectorize(&vec2);
    KALDI_ASSERT(uc2->Info() == uc3->Info() && VecVec(vec2, vec2) == 0.0);

    delete uc2;
    delete uc3;
  }
}

// tests the properties kPropagateAdds, kBackpropAdds,
// kBackpropNeedsInput, kBackpropNeedsOutput.
void TestSimpleComponentPropagateProperties(const Component &c) {
  int32 properties = c.Properties();
  Component *c_copy = NULL, *c_copy_scaled = NULL;
  if (RandInt(0, 1) == 0)
    c_copy = c.Copy();  // This will test backprop with an updatable component.
  if (RandInt(0, 1) == 0 &&
      (properties & kLinearInParameters)) {
    c_copy_scaled = c.Copy();  // This will test backprop with an updatable component.
    c_copy_scaled->Scale(0.5);
  }
  MatrixStrideType input_stride_type = (c.Properties()&kInputContiguous) ?
      kStrideEqualNumCols : kDefaultStride;
  MatrixStrideType output_stride_type = (c.Properties()&kOutputContiguous) ?
      kStrideEqualNumCols : kDefaultStride;

  int32 input_dim = c.InputDim(),
      output_dim = c.OutputDim(),
      num_rows = RandInt(1, 100);
  CuMatrix<BaseFloat> input_data(num_rows, input_dim, kUndefined,
                                 input_stride_type);
  input_data.SetRandn();
  CuMatrix<BaseFloat> input_data_scaled(num_rows, input_dim, kUndefined,
                                        input_stride_type),
      output_data3(num_rows, input_dim, kSetZero,
                   output_stride_type);
  input_data_scaled.CopyFromMat(input_data);
  output_data3.CopyFromMat(input_data);
  CuMatrix<BaseFloat>
      output_data1(num_rows, output_dim, kSetZero, output_stride_type),
      output_data2(num_rows, output_dim, kSetZero, output_stride_type),
      output_data4(num_rows, output_dim, kSetZero, output_stride_type),
      output_data5(num_rows, output_dim, kSetZero, output_stride_type);
  output_data2.Add(1.0);
  input_data_scaled.Scale(2.0);

  if ((properties & kPropagateAdds) && (properties & kPropagateInPlace)) {
    KALDI_ERR << "kPropagateAdds and kPropagateInPlace flags are incompatible.";
  }

  c.Propagate(NULL, input_data, &output_data1);
  c.Propagate(NULL, input_data, &output_data2);
  if (properties & kPropagateInPlace) {
    c.Propagate(NULL, output_data3, &output_data3);
    if (!output_data1.ApproxEqual(output_data3)) {
      KALDI_ERR << "Test of kPropagateInPlace flag for component of type "
                << c.Type() << " failed.";
    }
  }
  if (properties & kPropagateAdds)
    output_data2.Add(-1.0); // remove the offset
  AssertEqual(output_data1, output_data2);

  if (c_copy_scaled) {
    c_copy_scaled->Propagate(NULL, input_data, &output_data4);
    output_data4.Scale(2.0);  // we scaled the parameters by 0.5 above, and the
    // output is supposed to be linear in the parameter value.
    AssertEqual(output_data1, output_data4);
  }
  if (properties & kLinearInInput) {
    c.Propagate(NULL, input_data_scaled, &output_data5);
    output_data5.Scale(0.5);
    AssertEqual(output_data1, output_data5);
  }


  CuMatrix<BaseFloat> output_deriv(num_rows, output_dim, kSetZero, output_stride_type);
  output_deriv.SetRandn();
  CuMatrix<BaseFloat> input_deriv1(num_rows, input_dim, kSetZero, input_stride_type),
      input_deriv2(num_rows, input_dim, kSetZero, input_stride_type);
  CuMatrix<BaseFloat> input_deriv3(num_rows, output_dim, kSetZero, input_stride_type);
  input_deriv3.CopyFromMat(output_deriv);

  input_deriv2.Add(1.0);
  CuMatrix<BaseFloat> empty_mat;

  // test with input_deriv1 that's zero
  c.Backprop("foobar", NULL,
             ((properties & kBackpropNeedsInput) ? input_data : empty_mat),
             ((properties & kBackpropNeedsOutput) ? output_data1 : empty_mat),
             output_deriv,
             c_copy,
             &input_deriv1);
  // test with input_deriv2 that's all ones.
  c.Backprop("foobar", NULL,
             ((properties & kBackpropNeedsInput) ? input_data : empty_mat),
             ((properties & kBackpropNeedsOutput) ? output_data1 : empty_mat),
             output_deriv,
             c_copy,
             &input_deriv2);
  // test backprop in place, if supported.
  if (properties & kBackpropInPlace) {
    c.Backprop("foobar", NULL,
               ((properties & kBackpropNeedsInput) ? input_data : empty_mat),
               ((properties & kBackpropNeedsOutput) ? output_data1 : empty_mat),
               input_deriv3,
               c_copy,
               &input_deriv3);
  }

  if (properties & kBackpropAdds)
    input_deriv2.Add(-1.0);  // subtract the offset.
  AssertEqual(input_deriv1, input_deriv2);
  if (properties & kBackpropInPlace)
    AssertEqual(input_deriv1, input_deriv3);
  delete c_copy;
  delete c_copy_scaled;
}

bool TestSimpleComponentDataDerivative(const Component &c,
                                       BaseFloat perturb_delta) {
  MatrixStrideType input_stride_type = (c.Properties()&kInputContiguous) ?
      kStrideEqualNumCols : kDefaultStride;
  MatrixStrideType output_stride_type = (c.Properties()&kOutputContiguous) ?
      kStrideEqualNumCols : kDefaultStride;

  int32 input_dim = c.InputDim(),
      output_dim = c.OutputDim(),
      num_rows = RandInt(1, 100);
  int32 properties = c.Properties();
  CuMatrix<BaseFloat> input_data(num_rows, input_dim, kSetZero, input_stride_type),
      output_data(num_rows, output_dim, kSetZero, output_stride_type),
      output_deriv(num_rows, output_dim, kSetZero, output_stride_type);
  input_data.SetRandn();
  output_deriv.SetRandn();

  c.Propagate(NULL, input_data, &output_data);

  CuMatrix<BaseFloat> input_deriv(num_rows, input_dim, kSetZero, input_stride_type),
      empty_mat;
  c.Backprop("foobar", NULL,
             ((properties & kBackpropNeedsInput) ? input_data : empty_mat),
             ((properties & kBackpropNeedsOutput) ? output_data : empty_mat),
             output_deriv, NULL, &input_deriv);

  int32 test_dim = 3;
  BaseFloat original_objf = TraceMatMat(output_deriv, output_data, kTrans);
  Vector<BaseFloat> measured_objf_change(test_dim),
      predicted_objf_change(test_dim);
  for (int32 i = 0; i < test_dim; i++) {
    CuMatrix<BaseFloat> perturbed_input_data(num_rows, input_dim,
                                             kSetZero, input_stride_type),
        perturbed_output_data(num_rows, output_dim,
                              kSetZero, output_stride_type);
    perturbed_input_data.SetRandn();
    perturbed_input_data.Scale(perturb_delta);
    // at this point, perturbed_input_data contains the offset at the input data.
    predicted_objf_change(i) = TraceMatMat(perturbed_input_data, input_deriv,
                                           kTrans);
    perturbed_input_data.AddMat(1.0, input_data);
    c.Propagate(NULL, perturbed_input_data, &perturbed_output_data);
    measured_objf_change(i) = TraceMatMat(output_deriv, perturbed_output_data,
                                          kTrans) - original_objf;
  }
  KALDI_LOG << "Predicted objf-change = " << predicted_objf_change;
  KALDI_LOG << "Measured objf-change = " << measured_objf_change;
  BaseFloat threshold = 0.1;
  bool ans = ApproxEqual(predicted_objf_change, measured_objf_change, threshold);
  if (!ans)
    KALDI_WARN << "Data-derivative test failed, component-type="
               << c.Type() << ", input-dim=" << input_dim
               << ", output-dim=" << output_dim;
  if (c.Type() == "NormalizeComponent" && input_dim == 1) {
    // derivatives are mathematically zero, but the measured and predicted
    // objf have different roundoff and the relative differences are large.
    // this is not unexpected.
    KALDI_LOG << "Accepting deriv differences since it is NormalizeComponent "
              << "with dim=1.";
    return true;
  }
  else if (c.Type() == "ClipGradientComponent") {
    KALDI_LOG << "Accepting deriv differences since "
              << "it is ClipGradientComponent.";
    return true;
  }
  return ans;
}


// if test_derivative == false then the test only tests that the update
// direction is downhill.  if true, then we measure the actual model-derivative
// and check that it's accurate.
// returns true on success, false on test failure.
bool TestSimpleComponentModelDerivative(const Component &c,
                                        BaseFloat perturb_delta,
                                        bool test_derivative) {
  int32 input_dim = c.InputDim(),
      output_dim = c.OutputDim(),
      num_rows = RandInt(1, 100);
  int32 properties = c.Properties();
  if ((properties & kUpdatableComponent) == 0) {
    // nothing to test.
    return true;
  }
  MatrixStrideType input_stride_type = (c.Properties()&kInputContiguous) ?
      kStrideEqualNumCols : kDefaultStride;
  MatrixStrideType output_stride_type = (c.Properties()&kOutputContiguous) ?
      kStrideEqualNumCols : kDefaultStride;

  CuMatrix<BaseFloat> input_data(num_rows, input_dim, kSetZero, input_stride_type),
      output_data(num_rows, output_dim, kSetZero, output_stride_type),
      output_deriv(num_rows, output_dim, kSetZero, output_stride_type);
  input_data.SetRandn();
  output_deriv.SetRandn();

  c.Propagate(NULL, input_data, &output_data);

  BaseFloat original_objf = TraceMatMat(output_deriv, output_data, kTrans);

  Component *c_copy = c.Copy();

  const UpdatableComponent *uc = dynamic_cast<const UpdatableComponent*>(&c);
  UpdatableComponent *uc_copy = dynamic_cast<UpdatableComponent*>(c_copy);
  KALDI_ASSERT(uc != NULL && uc_copy != NULL);
  if (test_derivative) {
    bool is_gradient = true;
    uc_copy->SetZero(is_gradient);
  }

  CuMatrix<BaseFloat> input_deriv(num_rows, input_dim,
                                  kSetZero, input_stride_type),
      empty_mat;
  c.Backprop("foobar", NULL,
             ((properties & kBackpropNeedsInput) ? input_data : empty_mat),
             ((properties & kBackpropNeedsOutput) ? output_data : empty_mat),
             output_deriv, c_copy,
             (RandInt(0, 1) == 0 ? &input_deriv : NULL));

  if (!test_derivative) { // Just testing that the model update is downhill.
    CuMatrix<BaseFloat> new_output_data(num_rows, output_dim,
                                        kSetZero, output_stride_type);
    c_copy->Propagate(NULL, input_data, &new_output_data);

    BaseFloat new_objf = TraceMatMat(output_deriv, new_output_data, kTrans);
    bool ans = (new_objf > original_objf);
    if (!ans) {
      KALDI_WARN << "After update, new objf is not better than the original objf: "
                 << new_objf << " <= " << original_objf;
    }
    delete c_copy;
    return ans;
  } else {
    // check that the model derivative is accurate.
    int32 test_dim = 3;

    Vector<BaseFloat> measured_objf_change(test_dim),
        predicted_objf_change(test_dim);
    for (int32 i = 0; i < test_dim; i++) {
      CuMatrix<BaseFloat> perturbed_output_data(num_rows, output_dim,
                                                kSetZero, output_stride_type);
      Component *c_perturbed = c.Copy();
      UpdatableComponent *uc_perturbed =
          dynamic_cast<UpdatableComponent*>(c_perturbed);
      KALDI_ASSERT(uc_perturbed != NULL);
      uc_perturbed->PerturbParams(perturb_delta);

      predicted_objf_change(i) = uc_copy->DotProduct(*uc_perturbed) -
          uc_copy->DotProduct(*uc);
      c_perturbed->Propagate(NULL, input_data, &perturbed_output_data);
      measured_objf_change(i) = TraceMatMat(output_deriv, perturbed_output_data,
                                            kTrans) - original_objf;
      delete c_perturbed;
    }
    KALDI_LOG << "Predicted objf-change = " << predicted_objf_change;
    KALDI_LOG << "Measured objf-change = " << measured_objf_change;
    BaseFloat threshold = 0.1;

    bool ans = ApproxEqual(predicted_objf_change, measured_objf_change,
                           threshold);
    if (!ans)
      KALDI_WARN << "Model-derivative test failed, component-type="
                 << c.Type() << ", input-dim=" << input_dim
                 << ", output-dim=" << output_dim;
    delete c_copy;
    return ans;
  }
}


void UnitTestNnetComponent() {
  for (int32 n = 0; n < 200; n++)  {
    Component *c = GenerateRandomSimpleComponent();
    KALDI_LOG << c->Info();
    TestNnetComponentIo(c);
    TestNnetComponentCopy(c);
    TestNnetComponentAddScale(c);
    TestNnetComponentVectorizeUnVectorize(c);
    TestNnetComponentUpdatable(c);
    TestSimpleComponentPropagateProperties(*c);
    if (!TestSimpleComponentDataDerivative(*c, 1.0e-04) &&
        !TestSimpleComponentDataDerivative(*c, 1.0e-03) &&
        !TestSimpleComponentDataDerivative(*c, 1.0e-05) &&
        !TestSimpleComponentDataDerivative(*c, 1.0e-06))
      KALDI_ERR << "Component data-derivative test failed";

    if (!TestSimpleComponentModelDerivative(*c, 1.0e-04, false) &&
        !TestSimpleComponentModelDerivative(*c, 1.0e-03, false) &&
        !TestSimpleComponentModelDerivative(*c, 1.0e-06, false))
      KALDI_ERR << "Component downhill-update test failed";

    if (!TestSimpleComponentModelDerivative(*c, 1.0e-04, true) &&
        !TestSimpleComponentModelDerivative(*c, 1.0e-03, true) &&
        !TestSimpleComponentModelDerivative(*c, 1.0e-05, true) &&
        !TestSimpleComponentModelDerivative(*c, 1.0e-06, true))
      KALDI_ERR << "Component model-derivative test failed";

    delete c;
  }
}

} // namespace nnet3
} // namespace kaldi

int main() {
  using namespace kaldi;
  using namespace kaldi::nnet3;
  TestStringsApproxEqual();
  for (kaldi::int32 loop = 0; loop < 2; loop++) {
#if HAVE_CUDA == 1
    if (loop == 0)
      CuDevice::Instantiate().SelectGpuId("no");
    else
      CuDevice::Instantiate().SelectGpuId("yes");
#endif
    UnitTestNnetComponent();
  }

  KALDI_LOG << "Nnet component ntests succeeded.";

  return 0;
}
